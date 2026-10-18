#ifndef WINUC_CHAT_TOOL_H
#define WINUC_CHAT_TOOL_H

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif
#include<bits/stdc++.h>
#include"sqlite3.h"
#include"httplib.h"
#include"json.hpp"

using namespace std;
using namespace httplib;
using nlohmann::json;
extern sqlite3 *db;

struct SqliteArg{
	int type;
	long long val_int;
	string val_text;
	SqliteArg(){}
	SqliteArg(long long val)
	{
		type=SQLITE_INTEGER;
		val_int=val;
	}
	SqliteArg(int val)
	{
		type=SQLITE_INTEGER;
		val_int=val;
	}
	SqliteArg(string val)
	{
		type=SQLITE_TEXT;
		val_text=val;
	}
	SqliteArg(const char* val)
	{
		type=SQLITE_TEXT;
		val_text=val;
	}
};
class SafeStmt{
	sqlite3_stmt *stmt;
	bool prepared;
public:
	SafeStmt()
	{
		stmt=nullptr;
		prepared=false;
	}
	~SafeStmt()
	{
		if(prepared)
			sqlite3_finalize(stmt);
	}
	void prepare(sqlite3 *d,string sql)
	{
		if(prepared)
			sqlite3_finalize(stmt);
		int tmp=sqlite3_prepare_v2(d,sql.c_str(),-1,&stmt,nullptr);
		if(tmp!=SQLITE_OK)
		{
			stmt=nullptr;
			throw runtime_error("Prepare sql error:"+sql);
		}
		prepared=true;
	}
	void bind(vector<SqliteArg>args)
	{
		if(!prepared)
			return;
		for(int i=0;i<args.size();i++)
		{
			switch(args[i].type)
			{
			case SQLITE_INTEGER:
				sqlite3_bind_int64(stmt,i+1,args[i].val_int);
				break;
			case SQLITE_TEXT:
				sqlite3_bind_text(stmt,i+1,args[i].val_text.c_str(),-1,SQLITE_TRANSIENT);
				break;
			default:
				throw runtime_error("Undefined arg type");
				break;
			}
		}
	}
	pair<int,vector<SqliteArg>> step()
	{
		if(!prepared)
			throw runtime_error("Step before prepare");
		pair<int,vector<SqliteArg>> ret;
		ret.first=sqlite3_step(stmt);
		if(ret.first!=SQLITE_ROW)
			return ret;
		int argCnt=sqlite3_column_count(stmt);
		ret.second.resize(argCnt);
		for(int i=0;i<argCnt;i++)
		{
			ret.second[i].type=sqlite3_column_type(stmt,i);
			switch(ret.second[i].type)
			{
			case SQLITE_INTEGER:
				ret.second[i].val_int=sqlite3_column_int64(stmt,i);
				break;
			case SQLITE_TEXT:
				ret.second[i].val_text=(char*)sqlite3_column_text(stmt,i);
				break;
			default:
				throw runtime_error("Undefined arg type");
				break;
			}
		}
		return ret;
	}
	void reset()
	{
		if(!prepared)
			return;
		sqlite3_reset(stmt);
	}
	void finalize()
	{
		if(!prepared)
			return;
		sqlite3_finalize(stmt);
		prepared=false;
	}
};

string getTime()
{
	auto toString_2 = [](int x) {
		return string() + char(x / 10 + '0') + char(x % 10 + '0');
	};
	time_t tmp = time(nullptr);
	tm t;                            // 栈上分配，线程独立
#ifdef _WIN32
	localtime_s(&t, &tmp);         // Windows：目标在前，时间源在后
#else
	localtime_r(&tmp, &t);        // Linux/Unix：时间源在前，目标在后
#endif         // POSIX 线程安全版
	// Windows 下用 localtime_s(&t, &tmp);
	int year  = t.tm_year + 1900;
	int month = t.tm_mon + 1;
	int day   = t.tm_mday;
	int hour  = t.tm_hour;
	int minute = t.tm_min;
	int second = t.tm_sec;
	return to_string(year) + '.' +
	to_string(month) + '.' +
	to_string(day) + ' ' +
	toString_2(hour) + ':' + toString_2(minute) + ':' + toString_2(second);
}

string readFullFile(const string &path,bool isBinary=false)
{
	ifstream fin;
	if(isBinary) fin.open(path,ios::binary);
	else fin.open(path);
	if(!fin.is_open())
	{
		printf("    %s[ERROR]Can't open file:%s\n",getTime().c_str(),(path+(isBinary?"(with binary mode)":"")).c_str());
		return string();
	}
	char c;
	string res;
	while(fin.get(c)) res+=c;
	fin.close();
	return res;
}

string getStr(json js,const string &key)
{
	if(!js.contains(key)||!js[key].is_string())
		return "";
	return js[key].get<string>();
}
int getInt(json js,const string &key)
{
	if(!js.contains(key)||!js[key].is_number())
		return 0;
	return js[key].get<int>();
}
bool getBool(json js,const string &key)
{
	if(!js.contains(key)||!js[key].is_boolean())
		return false;
	return js[key].get<bool>();
}

#endif
