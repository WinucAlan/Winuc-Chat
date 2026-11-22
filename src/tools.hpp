#ifndef WINUC_CHAT_TOOL_H
#define WINUC_CHAT_TOOL_H

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#include<winsock2.h>
#include<windows.h>
#include<bcrypt.h>
#else
#include <openssl/evp.h>
#endif
#include<bits/stdc++.h>
#include"sqlite3.h"
#include"httplib.h"
#include"nlohmann/json.hpp"

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

namespace SHAByDeepSeek {
#ifdef _WIN32
	// 检查 NTSTATUS，失败则抛异常
	inline void CheckNtStatus(NTSTATUS status, const char* what) {
	    if (!BCRYPT_SUCCESS(status)) {
	        throw std::runtime_error(std::string("SHA256 ") + what + " failed");
	    }
	}
	
	std::string ToHex(const unsigned char* d, size_t n) {
	    static const char* kHex = "0123456789abcdef";
	    std::string s;
	    s.reserve(n * 2);
	    for (size_t i = 0; i < n; ++i) {
	        s.push_back(kHex[d[i] >> 4]);
	        s.push_back(kHex[d[i] & 0x0F]);
	    }
	    return s;
	}
	
	// RAII：自动管理 alg / hash 句柄
	struct CngSha256 {
	    BCRYPT_ALG_HANDLE alg = nullptr;
	    BCRYPT_HASH_HANDLE hash = nullptr;
	    std::vector<unsigned char> hashObject;
	    DWORD hashLength = 0;
	
	    CngSha256() {
	        CheckNtStatus(
	            BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0),
	            "open algorithm");
	
	        DWORD cbData = 0;
	        DWORD objSize = 0;
	        CheckNtStatus(
	            BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
	                              reinterpret_cast<PUCHAR>(&objSize),
	                              sizeof(objSize), &cbData, 0),
	            "get object length");
	        hashObject.resize(objSize);
	
	        CheckNtStatus(
	            BCryptGetProperty(alg, BCRYPT_HASH_LENGTH,
	                              reinterpret_cast<PUCHAR>(&hashLength),
	                              sizeof(hashLength), &cbData, 0),
	            "get hash length");
	
	        CheckNtStatus(
	            BCryptCreateHash(alg, &hash, hashObject.data(), objSize,
	                             nullptr, 0, 0),
	            "create hash");
	    }
	
	    ~CngSha256() {
	        if (hash) BCryptDestroyHash(hash);
	        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
	    }
	
	    CngSha256(const CngSha256&) = delete;
	    CngSha256& operator=(const CngSha256&) = delete;
	
	    void Update(const void* data, size_t len) {
	        if (len == 0) return;
	        CheckNtStatus(
	            BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<void*>(data)),
	                           static_cast<ULONG>(len), 0),
	            "hash data");
	    }
	
	    std::string Finish() {
	        std::vector<unsigned char> digest(hashLength);
	        CheckNtStatus(
	            BCryptFinishHash(hash, digest.data(), hashLength, 0),
	            "finish hash");
	        return ToHex(digest.data(), digest.size());
	    }
	};
	
	// 1) 任意内存块
	std::string sha256(const void* data, size_t len) {
	    CngSha256 c;
	    c.Update(data, len);
	    return c.Finish();
	}
			
	// 2) 字符串
	std::string sha256(const std::string& s) {
	    return sha256(s.data(), s.size());
	}
			
	// 3) 流：分块读取，适合大文件
	std::string sha256(std::istream& in) {
	    CngSha256 c;
	    char buf[8192];
	    while (in) {
	        in.read(buf, sizeof(buf));
	        c.Update(buf, static_cast<size_t>(in.gcount()));
	    }
	    if (in.bad()) throw std::runtime_error("read error");
	    return c.Finish();
	}

#else
	string toHex(const unsigned char* d, unsigned int n) {
		static const char* kHex = "0123456789abcdef";
		string s;
		s.reserve(n * 2);
		for (unsigned int i = 0; i < n; ++i) {
			s.push_back(kHex[d[i] >> 4]);
			s.push_back(kHex[d[i] & 0x0F]);
		}
		return s;
	}
	
// RAII：构造即 Init，析构自动 Free
	struct EvpMdCtx {
		EVP_MD_CTX* ctx = EVP_MD_CTX_new();
		EvpMdCtx() {
			if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1)
				throw runtime_error("SHA256 init failed");
		}
		~EvpMdCtx() { EVP_MD_CTX_free(ctx); }
		EvpMdCtx(const EvpMdCtx&) = delete;
		EvpMdCtx& operator=(const EvpMdCtx&) = delete;
	};
	
	void update(EVP_MD_CTX* ctx, const void* data, size_t len) {
		if (len && EVP_DigestUpdate(ctx, data, len) != 1)
			throw runtime_error("SHA256 update failed");
	}
	
	string finalize(EVP_MD_CTX* ctx) {
		unsigned char digest[EVP_MAX_MD_SIZE];
		unsigned int len = 0;
		if (EVP_DigestFinal_ex(ctx, digest, &len) != 1)
			throw runtime_error("SHA256 final failed");
		return toHex(digest, len);
	}
	
	// 1) 任意内存块
	string sha256(const void* data, size_t len) {
		EvpMdCtx c;
		update(c.ctx, data, len);
		return finalize(c.ctx);
	}
	
	// 2) 字符串
	string sha256(const string& s) {
		return sha256(s.data(), s.size());
	}
	
	// 3) 流：文件/大文件分块读取
	string sha256(istream& in) {
		EvpMdCtx c;
		char buf[8192];
		while (in) {
			in.read(buf, sizeof(buf));
			update(c.ctx, buf, static_cast<size_t>(in.gcount()));
		}
		if (in.bad()) throw runtime_error("read error");
		return finalize(c.ctx);
	}
	
	// 4) 文件路径辅助（不想手动开 ifstream 时用）
	string sha256File(const string& path) {
		ifstream f(path, ios::binary);
		if (!f) throw runtime_error("open failed: " + path);
		return sha256(f);
	}
#endif
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
