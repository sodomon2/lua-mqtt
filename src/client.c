/* 
** lua-mqtt
** Copyright (C) 2017 tacigar
*/

#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define MQTT_CLIENT_CLASS "mqtt.client"

/*
** Synchronous client class.
*/
typedef struct Client
{
    MQTTClient m_client;
} Client;

/*
** This function creates an MQTT client ready for connection to the specified 
** server and using the specified persistent storage.
*/
static int clientCreate(lua_State *L)
{
    int rc;
    const char *serverURI = luaL_checkstring(L, 1);
    const char *clientID = luaL_checkstring(L, 2);
    
    int persistenceType = MQTTCLIENT_PERSISTENCE_NONE;
    void *persistenceContext = NULL;

    Client *client = (Client *)lua_newuserdata(L, sizeof(Client));
    rc = MQTTClient_create(&(client->m_client), serverURI, clientID, 
                           persistenceType, persistenceContext);

    if (rc != MQTTCLIENT_SUCCESS) {
        lua_pushnil(L);
        lua_pushfstring(L, "cannot create a new client : %d", rc);
        return 2;
    }

    luaL_getmetatable(L, MQTT_CLIENT_CLASS);
    lua_setmetatable(L, -2);
    return 1;
}

/*
** This function attempts to connect a previously-created client to an MQTT 
** server using the specified options.
*/
static int clientConnect(lua_State *L)
{
    int rc;
    Client *client;
    client = (Client *)luaL_checkudata(L, 1, MQTT_CLIENT_CLASS);
    
    MQTTClient_connectOptions connOpts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions sslOpts = MQTTClient_SSLOptions_initializer;
    MQTTClient_willOptions willOpts = MQTTClient_willOptions_initializer;

    lua_pushnil(L);
    while (lua_next(L, 2)) {
        const char *key = lua_tostring(L, -2);
        
        if (strcmp(key, "keepAliveInterval") == 0) {
            connOpts.keepAliveInterval = luaL_checkinteger(L, -1);
        } else if (strcmp(key, "cleanSession") == 0) {
            connOpts.cleansession = lua_toboolean(L, -1);
        } else if (strcmp(key, "reliable") == 0) {
            connOpts.reliable = lua_toboolean(L, -1);
        } else if (strcmp(key, "username") == 0) {
            connOpts.username = luaL_checkstring(L, -1);
        } else if (strcmp(key, "password") == 0) {
            connOpts.password = luaL_checkstring(L, -1);
        } else if (strcmp(key, "connectTimeout") == 0) {
            connOpts.connectTimeout = luaL_checkinteger(L, -1);
        } else if (strcmp(key, "retryInterval") == 0) {
            connOpts.retryInterval = luaL_checkinteger(L, -1);
        } else if (strcmp(key, "mqttVersion") == 0) {
            connOpts.MQTTVersion = luaL_checkinteger(L, -1);
        } else if (strcmp(key, "will") == 0) {
            lua_pushnil(L);
            while (lua_next(L, -1)) {
                const char *key = lua_tostring(L, -2);
                
                if (strcmp(key, "topicName") == 0) {
                    willOpts.topicName = luaL_checkstring(L, -1);
                } else if (strcmp(key, "message") == 0) {
                    willOpts.message = luaL_checkstring(L, -1);
                } else if (strcmp(key, "retained") == 0) {
                    willOpts.retained = lua_toboolean(L, -1);
                } else if (strcmp(key, "qos") == 0) {
                    willOpts.qos = luaL_checkinteger(L, -1);
                }
                lua_pop(L, 1);
            }
            connOpts.will = &willOpts;

        } else if (strcmp(key, "ssl") == 0) {
            lua_pushnil(L);
            while (lua_next(L, -1)) {
                const char *key = lua_tostring(L, -2);

                if (strcmp(key, "trustStore") == 0) {
                    sslOpts.trustStore = luaL_checkstring(L, -1);
                } else if (strcmp(key, "keyStore") == 0) {
                    sslOpts.keyStore = luaL_checkstring(L, -1);
                } else if (strcmp(key, "privateKey") == 0) {
                    sslOpts.privateKey = luaL_checkstring(L, -1);
                } else if (strcmp(key, "privateKeyPassword") == 0) {
                    sslOpts.privateKeyPassword = luaL_checkstring(L, -1);
                } else if (strcmp(key, "enabledCipherSuites") == 0) {
                    sslOpts.enabledCipherSuites = luaL_checkstring(L, -1);
                } else if (strcmp(key, "enableServerCertAuth") == 0) {
                    sslOpts.enableServerCertAuth = lua_toboolean(L, -1);
                }
                lua_pop(L, 1);
            }
            connOpts.ssl = &sslOpts;

        } else if (strcmp(key, "serverURIs") == 0) {
            int i;
            size_t len = lua_rawlen(L, -1);

            char **serverURIs = malloc(len * sizeof(char *));;
            connOpts.serverURIcount = len;
            
            for (i = 0; i < len; ++i) {
                lua_pushnumber(L, i + 1);
                lua_gettable(L, -2);

                const char *serverURI = luaL_checkstring(L, -1);
                char *buffer = (char *)malloc(sizeof(char) * (strlen(serverURI) + 1));
                strcpy(buffer, serverURI);
                
                serverURIs[i] = buffer;
                lua_pop(L, 1);
            }
            connOpts.serverURIs = serverURIs;
        } 
        lua_pop(L, 1);
    }

    rc = MQTTClient_connect(client->m_client, &connOpts);

    if (connOpts.serverURIs) {
        int i;
        for (i = 0; i < connOpts.serverURIcount; ++i) {
            free(connOpts.serverURIs[i]);
        }
        free((char **)connOpts.serverURIs);
    }

    switch (rc) {
        case -1:
            lua_pushstring(L, "connecttion error");
            return 1;
        case 1: 
            lua_pushstring(L, "unacceptable protocol version");
            return 1;
        case 2:
            lua_pushstring(L, "identifier rejected");
            return 1;
        case 3:
            lua_pushstring(L, "server unavailable");
            return 1;
        case 4:
            lua_pushstring(L, "bad user name or password");
            return 1;
        case 5:
            lua_pushstring(L, "not authorized");
            return 1;
    }
    return 0;
}

/*
** Module entry point.
*/
LUALIB_API int luaopen_mqtt_Client(lua_State *L)
{
    struct luaL_Reg *ptr;
    struct luaL_Reg methods[] = {
        { "connect", clientConnect },
        { NULL, NULL }
    };

    luaL_newmetatable(L, MQTT_CLIENT_CLASS);

    lua_pushstring(L, "__index");
    lua_newtable(L);
    ptr = methods;
    do {
        lua_pushstring(L, ptr->name);
        lua_pushcfunction(L, ptr->func);
        lua_rawset(L, -3);
        ptr++;
    } while (ptr->name);
    lua_rawset(L, -3);

    lua_pop(L, 1); /* pop the metatable. */

    lua_newtable(L);
    lua_pushstring(L, "new");
    lua_pushcfunction(L, clientCreate);
    lua_rawset(L, -3);

    return 1;
}
