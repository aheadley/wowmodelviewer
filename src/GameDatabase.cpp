/*
 * GameDatabase.cpp
 *
 *  Created on: 9 nov. 2014
 *      Author: Jerome
 */

#include "GameDatabase.h"

#include "dbcfile.h"
#include "logger\Logger.h"

#include <QDomDocument>
#include <QDomElement>
#include <QDomNamedNodeMap>
#include <QFile>


GameDatabase::~GameDatabase()
{
  if(m_db)
    sqlite3_close(m_db);
}


GameDatabase::GameDatabase()
: m_db(NULL)
{

}

bool GameDatabase::initFromXML(const std::string & file)
{
   int rc = sqlite3_open(":memory:", &m_db);

   if( rc )
   {
     LOG_INFO << "Can't open database:" << sqlite3_errmsg(m_db);
     return false;
   }
   else
   {
     LOG_INFO << "Opened database successfully";
   }

   return createDatabaseFromXML(file);
}

sqlResult GameDatabase::sqlQuery(const std::string & query)
{
  sqlResult result;

  char *zErrMsg = 0;
  int rc = sqlite3_exec(m_db, query.c_str(), GameDatabase::treatQuery, (void *)&result, &zErrMsg);
  if( rc != SQLITE_OK )
  {
    LOG_ERROR << "Querying in database" << query.c_str();
    LOG_ERROR << "SQL error:" << zErrMsg;
    sqlite3_free(zErrMsg);
    result.valid = false;
  }
  else
  {
    result.valid = true;
  }

  return result;
}

int GameDatabase::treatQuery(void *resultPtr, int nbcols, char ** vals , char ** cols)
{
  //std::cout << " !!!!! " <<  __FUNCTION__ << "!!!!!!" << std::endl;

  sqlResult * r = (sqlResult *)resultPtr;
  if(!r)
    return 1;

  std::vector<std::string> values;
  // update columns
  for(int i=0; i<nbcols; i++)
    values.push_back(vals[i]);

  r->values.push_back(values);
  r->nbcols = nbcols;

  return 0;
}

bool GameDatabase::createDatabaseFromXML(const std::string & file)
{
  QDomDocument doc;

  QFile f(file.c_str());
  f.open(QIODevice::ReadOnly);
  doc.setContent(&f);
  f.close();

  QDomElement docElem = doc.documentElement();

  QDomElement e = docElem.firstChildElement();

  while(!e.isNull())
  {
    if(!createTableFromXML(e))
    {
      LOG_ERROR << "Impossible to create table" << e.tagName();
    }
    else
    {
      LOG_INFO << "Table creation succeed for" << e.tagName();
      std::string gamedbfile = e.firstChildElement("gamefile").attributes().namedItem("name").nodeValue().toStdString();
      if(gamedbfile != "")
      {
        LOG_INFO << "Opening game database file" << gamedbfile.c_str();
        if(!fillTableFromGameFile(e.tagName().toStdString(), gamedbfile))
        {
          LOG_ERROR << "Error during filling of database" << e.tagName();
        }
      }
      else
      {
        LOG_ERROR << "Fail to find gamefile attribute";
      }
    }

    e = e.nextSiblingElement();
  }
  return true;
}

bool GameDatabase::createTableFromXML(const QDomElement & elem)
{
  QDomElement child = elem.firstChildElement();
  QDomElement lastChild = elem.lastChildElement();

  std::string curTable = elem.nodeName().toStdString();
  std::string create = "CREATE TABLE "+curTable+" (";

  while (!child.isNull())
  {
    QDomNamedNodeMap attributes = child.attributes();

    // search if name and type are here
    QDomNode name = attributes.namedItem("name");
    QDomNode type = attributes.namedItem("type");

    if(!name.isNull() && !type.isNull())
    {
      std::string fieldName = name.nodeValue().toStdString();
      std::string fieldType = type.nodeValue().toStdString();
      create += fieldName;
      create += " ";
      create += fieldType;
      if(!attributes.namedItem("primary").isNull())
        create += " PRIMARY KEY NOT NULL";
      if(child != lastChild)
        create += ",";
      m_dbStruct[curTable][fieldName] = fieldType;
    }

    child = child.nextSiblingElement();
  }

  create += ");";

  sqlResult r = sqlQuery(create);

  return r.valid;
}

bool GameDatabase::fillTableFromGameFile(const std::string & table, const std::string & gamefile)
{
  DBCFile dbc(gamefile.c_str());
  dbc.open();

  std::map<std::string, std::string> tableStruct = m_dbStruct[table];

  std::string query = "INSERT INTO ";
  query += table;
  query += "(";
  int nbFields = tableStruct.size();
  int curfield = 0;
  for(std::map<std::string, std::string>::iterator it = tableStruct.begin();
      it != tableStruct.end();
      ++it,curfield++)
  {
    query += it->first;
    if(curfield != nbFields-1)
      query += ",";
  }

  query += ") VALUES";
  std::string queryBase = query;
  int record = 0;
  int nbRecord = dbc.getRecordCount();
  for(DBCFile::Iterator it = dbc.begin(), itEnd = dbc.end(); it != itEnd; ++it,record++)
  {
    std::set<std::string> fields = it->get(tableStruct);
    curfield=0;
    for(std::set<std::string>::iterator itFields = fields.begin();
        itFields != fields.end();
        ++itFields, curfield++)
    {
      if(curfield == 0)
        query += " (";
      query += "\"";
      query += *itFields;
      query += "\"";
      if(curfield != nbFields-1)
        query += ",";
      else
        query += ")";
    }
    // inserting all items at once make application crash,
    // so insert by chunk of 200 lines
    if(record%200 == 0)
    {
      query += ";";
      sqlResult r = sqlQuery(query);
      if(!r.valid)
        return false;
      query = queryBase;
    }
    else
    {
      if(record != nbRecord-1)
        query+=",";
    }
  }

  query += ";";
  sqlResult r = sqlQuery(query);
  return r.valid;
}
