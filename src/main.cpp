#include"tools.hpp"
#include"chat_core.hpp"
sqlite3 *db=nullptr;
int main()
{
	if(sqlite3_open("data_4.x.db",&db)!=SQLITE_OK)
	{
		printf("%s[ERROR]Can't open db file\n",getTime().c_str());
		return 1;
	}
	init();
	Server svr;
	svr.Get("/",[](const Request &req,Response &res){
		res.set_content(readFullFile("webpage/main.html"),"text/html");
	});
	svr.Get("/favicon.ico",[](const Request &req,Response &res){
		res.set_content(readFullFile("webpage/favicon.png"),"image/apng");
	});
	//chat_core
	svr.WebSocket("/ws",handle_ws);
	
	svr.set_logger([](const Request& req,const Response& res){
		string real_ip=req.remote_addr;
		if (req.has_header("X-Forwarded-For"))
		{
			real_ip=req.get_header_value("X-Forwarded-For");
			size_t pos=real_ip.find(',');
			if (pos!=string::npos)
				real_ip=real_ip.substr(0, pos);
		} 
		else if(req.has_header("X-Real-IP"))
			real_ip=req.get_header_value("X-Real-IP");
		printf("%s[INFO]New request:IP=%s Method=%s path=%s\n",getTime().c_str(),real_ip.c_str(),req.method.c_str(),req.path.c_str());
	});
	
	svr.listen("0.0.0.0",8888);
	printf("%s[INFO]Server listening...\n",getTime().c_str());
	return 0;
}
