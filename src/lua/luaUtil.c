#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include <xkbcommon/xkbcommon.h>

#include "../../include/server.h"
#include "../../include/lua/luaUtil.h"

lua_State *hellwm_luaInit()
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    return L;
}

int hellwm_luaLoadFile(lua_State *L, char *filename)
{
  if (luaL_loadfile(L, filename) || lua_pcall(L, 0, 0, 0))
  {
      hellwm_log(HELLWM_ERROR,
          "%s: %s",
          filename,
          lua_tostring(L, -1)
      );
      return 1;
  }
  return 0;
}

int hellwm_luaGetTable(lua_State *L, char *tableName)
{
  lua_getglobal(L, tableName);
  if (!lua_istable(L, -1))
  {
    hellwm_log(HELLWM_ERROR, "%s is not a table");
    return 1;
  }
  return 0;
}

void *hellwm_luaGetField(lua_State *L, char *fieldName, int lua_type)
{
  void *temp;

  lua_pushstring(L, fieldName);
  lua_gettable(L, -2);

  switch (lua_type)
  {
    case LUA_TNUMBER:
      if (lua_isnumber(L, -1))
      {
        int val = lua_tonumber(L,-1); 
        temp = (void *)&val;
      }
      else
      {
        int *val = NULL; 
        temp = (void*)&val;
      }
      break;

    case LUA_TSTRING:
      if (lua_isstring(L, -1))
      {
        const char *val = lua_tostring(L, -1);
        temp = (void*)val;
      }
      else
      {
        const char *val = NULL; 
        temp = (void*)val;
      }
      break;
    default:
      temp = NULL;
  } 
  lua_pop(L,1);
  return temp;
}

void hellwm_luaGetTableField(lua_State *L, char *tableName, char *fieldName)
{
  lua_getglobal(L, tableName);
  lua_pushstring(L, fieldName);
  lua_gettable(L, -2);

  printf("%s - %s: %f",tableName, fieldName, lua_tonumber(L, -1));

  lua_pop(L,1);
  lua_pop(L,1);
}
