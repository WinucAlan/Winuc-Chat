#ifndef WINUC_CHAT_CORE_H
#define WINUC_CHAT_CORE_H

#include"tools.hpp"

unordered_map<string,ws::WebSocket*>mp;
mutex mp_mtx;

bool handle_login(json js)
{
	if(!js.contains("username")||!js.contains("password"))
		return false;
	string username=getStr(js,"username"),password=getStr(js,"password");
	SafeStmt stmt;
	stmt.prepare(db,"SELECT password FROM user WHERE username = ? LIMIT 1;");
	stmt.bind({username});
	auto res=stmt.step();
	if(res.first==SQLITE_ROW)
	{
		string real_password=res.second[0].val_text;
		if(real_password==password)
			return true;
		else
			return false;
	}
	return false;
}

string handle_send_msg_group_main(json js,const string &username,const int &event_id)
{
	//获取用户上一次发送的消息
	SafeStmt stmt;
	stmt.prepare(db,"SELECT msg_content FROM group_main WHERE msg_sender = ? ORDER BY msg_id DESC LIMIT 1;");
	stmt.bind({username});
	auto res=stmt.step();
	if(res.first==SQLITE_ROW&&
	   res.second[0].val_text==((getStr(js,"msg_type")=="text")?getStr(js,"msg_content"):getStr(js,"url")))
		return json({{"event",event_id},{"success",false},{"error",u8"不能重复发送消息（为了防止刷屏谢谢配合喵）"}}).dump();
	//获取当前消息msg_id
	int msg_id=0;
	stmt.prepare(db,"SELECT COUNT(*) FROM group_main;");
	res=stmt.step();
	if(res.first==SQLITE_ROW)
		msg_id=res.second[0].val_int;
	msg_id++;
	//插入到group_main表
	stmt.prepare(db,"INSERT INTO group_main (msg_id, msg_type, msg_sender, msg_content, msg_time) VALUES (?, ?, ?, ?, ?);");
	stmt.bind({msg_id,getStr(js,"msg_type"),username,((getStr(js,"msg_type")=="text")?getStr(js,"msg_content"):getStr(js,"url")),(long long)time(0)});
	stmt.step();
	//向所有在线用户发送
	{
		lock_guard lock(mp_mtx);
		json tmp={{"type","new_message"},{"chat_id","group_main"},{"msg_id",msg_id}};
		for(auto online_user:mp)
			online_user.second->send(tmp.dump());
	}
	return json({{"event",event_id},{"success",true}}).dump();
}

string handle_send_msg_private(json js,const string &username,const int &event_id)
{
	string target=getStr(js,"chat_id").substr(8);
	//获取用户上一次发送的消息
	SafeStmt stmt;
	stmt.prepare(db,"SELECT msg_content FROM private_chat WHERE user_a = ? AND user_b = ? AND msg_sender = ? ORDER BY msg_id DESC LIMIT 1;");
	stmt.bind({min(username,target),max(username,target),username});
	auto res=stmt.step();
	if(res.first==SQLITE_ROW&&
	   res.second[0].val_text==((getStr(js,"msg_type")=="text")?getStr(js,"msg_content"):getStr(js,"url")))
		return json({{"event",event_id},{"success",false},{"error",u8"不能重复发送消息（为了防止刷屏谢谢配合喵）"}}).dump();
	//获取当前消息msg_id
	int msg_id=0;
	stmt.prepare(db,"SELECT COUNT(*) FROM private_chat WHERE user_a = ? AND user_b = ?;");
	stmt.bind({min(username,target),max(username,target)});
	res=stmt.step();
	if(res.first==SQLITE_ROW)
		msg_id=res.second[0].val_int;
	msg_id++;
	//插入到private_chat表
	stmt.prepare(db,"INSERT INTO private_chat (user_a, user_b, msg_id, msg_type, msg_sender, msg_content, msg_time) VALUES (?, ?, ?, ?, ?, ?, ?);");
	stmt.bind({min(username,target),max(username,target),msg_id,getStr(js,"msg_type"),username,getStr(js,"msg_type")=="text"?getStr(js,"msg_content"):getStr(js,"url"),(long long)time(0)});
	stmt.step();
	//更新好友列表
	stmt.prepare(db,"UPDATE user_friend SET last_time = ?, is_new = ? WHERE owner = ? AND friend = ?;");
	stmt.bind({(long long)time(0),0,username,target});
	stmt.step();
	stmt.reset();
	
	stmt.bind({(long long)time(0),1,target,username});
	stmt.step();
	//向对方发送
	{
		lock_guard lock(mp_mtx);
		if(mp.count(target))
		{
			json tmp={{"type","new_message"},{"chat_id","private_"+username},{"msg_id",msg_id}};
			mp[target]->send(tmp.dump());
		}
	}
	return json({{"event",event_id},{"success",true}}).dump();
}

string handle_send_msg(json js,const string &username,const int &event_id)
{
	if(getStr(js,"chat_id")=="group_main")
		return handle_send_msg_group_main(js,username,event_id);
	else if(getStr(js,"chat_id").substr(0,7)=="private")
		return handle_send_msg_private(js,username,event_id);
	else
		return json({{"event",event_id},{"success",false},{"error",u8"错误的请求参数\"chat_id\""}}).dump();
}

string handle_recall_group_main(json js,const string &username,const int &event_id)
{
	int recall_id=getInt(js,"msg_id");
	time_t nowTime=time(0);
	//查询指定消息的发送人、发送时间是否符合要求
	SafeStmt stmt;
	stmt.prepare(db,"SELECT msg_sender, msg_time FROM group_main WHERE msg_id = ? LIMIT 1;");
	stmt.bind({recall_id});
	auto res=stmt.step();
	if(res.first==SQLITE_ROW)
	{
		string msg_sender=res.second[0].val_text;
		time_t msg_time=res.second[1].val_int;
		if(msg_sender==username&&nowTime<=msg_time+1ll*30*60) //在30分钟内发送的消息
		{
			//将消息标记为recall
			stmt.prepare(db,"UPDATE group_main SET msg_type = ?, msg_content = ? WHERE msg_id = ?;");
			stmt.bind({"recall","",recall_id});
			stmt.step();
			//向所有在线用户发送
			{
				lock_guard lock(mp_mtx);
				json tmp={{"type","new_message"},{"chat_id","group_main"},{"msg_id",recall_id}};
				for(auto online_user:mp)
					online_user.second->send(tmp.dump());
			}
		}
		else
			return json({{"event",event_id},{"success",false},{"error",u8"这条消息不满足撤回要求"}}).dump();
	}
	else
		return json({{"event",event_id},{"success",false},{"error",u8"不存在这条消息"}}).dump();
	return json({{"event",event_id},{"success",true}}).dump();
}
string handle_recall_private(json js,const string &username,const int &event_id)
{
	int recall_id=getInt(js,"msg_id");
	time_t nowTime=time(0);
	//查询指定消息的发送人、发送时间是否符合要求
	string target=getStr(js,"chat_id").substr(8);
	SafeStmt stmt;
	stmt.prepare(db,"SELECT msg_sender, msg_time FROM private_chat WHERE user_a = ? AND user_b = ? AND msg_id = ? LIMIT 1;");
	stmt.bind({min(username,target),max(username,target),recall_id});
	auto res=stmt.step();
	if(res.first==SQLITE_ROW)
	{
		string msg_sender=res.second[0].val_text;
		time_t msg_time=res.second[1].val_int;
		if(msg_sender==username&&nowTime<=msg_time+1ll*30*60) //在30分钟内发送的消息
		{
			//将消息标记为recall
			stmt.prepare(db,"UPDATE private_chat SET msg_type = ?, msg_content = ? WHERE user_a = ? AND user_b = ? AND msg_id = ?;");
			stmt.bind({"recall","",min(username,target),max(username,target),recall_id});
			stmt.step();
			//更新好友列表
			stmt.prepare(db,"UPDATE user_friend SET last_time = ?, is_new = ? WHERE owner = ? AND friend = ?;");
			stmt.bind({(long long)time(0),0,username,target});
			stmt.step();
			stmt.reset();
			
			stmt.bind({(long long)time(0),1,target,username});
			stmt.step();
			//向对方发送
			{
				lock_guard lock(mp_mtx);
				if(mp.count(target))
				{
					json tmp={{"type","new_message"},{"chat_id","private_"+username},{"msg_id",recall_id}};
					mp[target]->send(tmp.dump());
				}
			}
		}
		else
			return json({{"event",event_id},{"success",false},{"error",u8"这条消息不符合撤回要求"}}).dump();
	}
	else
		return json({{"event",event_id},{"success",false},{"error",u8"不存在这条消息"}}).dump();
	return json({{"event",event_id},{"success",true}}).dump();
}
string handle_recall(json js,const string &username,const int &event_id)
{
	if(getStr(js,"chat_id")=="group_main")
		return handle_recall_group_main(js,username,event_id);
	else if(getStr(js,"chat_id").substr(0,7)=="private")
		return handle_recall_private(js,username,event_id);
	else
		return json({{"event",event_id},{"success",false},{"error",u8"错误的请求参数\"chat_id\""}}).dump();
}

string handle_get_history_group_main(const int &from,const int &to,const int &event_id)
{
	json arr;
	SafeStmt stmt;
	stmt.prepare(db,"SELECT * FROM group_main WHERE msg_id >= ? AND msg_id <= ? ORDER BY msg_id ASC;");
	stmt.bind({from,to});
	auto res=stmt.step();
	while(res.first==SQLITE_ROW)
	{
		json tmp;
		tmp["msg_id"]=res.second[0].val_int;
		tmp["msg_type"]=res.second[1].val_text;
		tmp["msg_sender"]=res.second[2].val_text;
		if(res.second[1].val_text=="text")
			tmp["content"]=res.second[3].val_text;
		else if(res.second[1].val_text!="recall")
			tmp["url"]=res.second[3].val_text;
		tmp["msg_time"]=res.second[4].val_int;
		arr.push_back(tmp);
		res=stmt.step();
	}
	return json({{"event",event_id},{"type","history"},{"messages",arr}}).dump();
}
string handle_get_history_private(const int &from,const int &to,const string &chat_id,const string &username,const int &event_id)
{
	string target=chat_id.substr(8);
	json arr;
	SafeStmt stmt;
	stmt.prepare(db,"SELECT * FROM private_chat WHERE msg_id >= ? AND msg_id <= ? AND user_a = ? AND user_b = ? ORDER BY msg_id ASC;");
	stmt.bind({from,to,min(username,target),max(username,target)});
	auto res=stmt.step();
	while(res.first==SQLITE_ROW)
	{
		json tmp;
		tmp["msg_id"]=res.second[2].val_int;
		tmp["msg_type"]=res.second[3].val_text;
		tmp["msg_sender"]=res.second[4].val_text;
		if(res.second[3].val_text=="text")
			tmp["content"]=res.second[5].val_text;
		else if(res.second[3].val_text!="recall")
			tmp["url"]=res.second[5].val_text;
		tmp["msg_time"]=res.second[6].val_int;
		arr.push_back(tmp);
		res=stmt.step();
	}
	return json({{"event",event_id},{"type","history"},{"messages",arr}}).dump();
}
string handle_get_history(const int &from,const int &to,const string &chat_id,const string &username,const int &event_id)
{
	if(chat_id=="group_main")
		return handle_get_history_group_main(from,to,event_id);
	else if(chat_id.substr(0,7)=="private")
		return handle_get_history_private(from,to,chat_id,username,event_id);
	else
		return json({{"event",event_id},{"success",false},{"error",u8"错误的请求参数\"chat_id\""}}).dump();
}

string handle_get_friend_list(const string &username,const int &event_id)
{
	SafeStmt stmt;
	stmt.prepare(db,"SELECT * FROM user_friend WHERE owner = ? ORDER BY last_time DESC;");
	stmt.bind({username});
	json arr;
	auto res=stmt.step();
	while(res.first==SQLITE_ROW)
	{
		arr.push_back({{"friend_name",res.second[1].val_text},{"is_new",res.second[2].val_int},{"last_time",res.second[3].val_int}});
		res=stmt.step();
	}
	return json({{"event",event_id},{"type","friend_list"},{"list",arr}}).dump();
}

string handle_get_friend_apply(const string &username,const int &event_id)
{
	json arr;
	SafeStmt stmt;
	stmt.prepare(db,"SELECT * FROM user_friend_apply WHERE owner = ?;");
	stmt.bind({username});
	auto res=stmt.step();
	while(res.first==SQLITE_ROW)
	{
		arr.push_back(res.second[1].val_text);
		res=stmt.step();
	}
	return json({{"event",event_id},{"type","friend_apply"},{"list",arr}}).dump();
}

void handle_tag_readed(const string &username,const string &chat_id)
{
	string target=chat_id.substr(8);
	SafeStmt stmt;
	stmt.prepare(db,"UPDATE user_friend SET is_new = ? WHERE owner = ? AND friend = ?;");
	stmt.bind({0,username,target});
	stmt.step();
}

string handle_add_friend(const string &username,const string &target,const int &event_id)
{
	if(username==target)
		return json({{"event",event_id},{"success",false},{"error",u8"不能和自己加好友"}}).dump();
	SafeStmt stmt;
	stmt.prepare(db,"SELECT 1 FROM user WHERE username = ? LIMIT 1;");
	stmt.bind({target});
	if(stmt.step().first!=SQLITE_ROW)
		return json({{"event",event_id},{"success",false},{"error",u8"对方不存在"}}).dump();
	stmt.prepare(db,"SELECT 1 FROM user_friend_apply WHERE owner = ? AND apply = ? LIMIT 1;");
	stmt.bind({username,target});
	if(stmt.step().first==SQLITE_ROW)
		return json({{"event",event_id},{"success",false},{"error",u8"对方已经向你发送请求"}}).dump();
	stmt.reset();
	stmt.bind({target,username});
	if(stmt.step().first==SQLITE_ROW)
		return json({{"event",event_id},{"success",false},{"error",u8"你已经向对方发送请求"}}).dump();
	stmt.prepare(db,"SELECT 1 FROM user_friend WHERE owner = ? AND friend = ? LIMIT 1;");
	stmt.bind({username,target});
	if(stmt.step().first==SQLITE_ROW)
		return json({{"event",event_id},{"success",false},{"error",u8"你和对方已经是好友"}}).dump();
	stmt.prepare(db,"INSERT INTO user_friend_apply (owner, apply) VALUES(?, ?);");
	stmt.bind({target,username});
	stmt.step();
	//发送给对方
	{
		lock_guard lock(mp_mtx);
		if(mp.count(target))
			mp[target]->send(json({{"type","new_apply"},{"from",username}}).dump());
	}
	return json({{"event",event_id},{"success",true}}).dump();
}

string handle_accept_friend_apply(const string &username,const string &target,const int &event_id)
{
	if(username==target)
		return json({{"event",event_id},{"success",false},{"error",u8"不能和自己加好友"}}).dump();
	SafeStmt stmt;
	stmt.prepare(db,"SELECT 1 FROM user_friend_apply WHERE owner = ? AND apply = ? LIMIT 1;");
	stmt.bind({username,target});
	if(stmt.step().first!=SQLITE_ROW)
		return json({{"event",event_id},{"success",false},{"error",u8"对方没有发送请求"}}).dump();
	stmt.prepare(db,"DELETE FROM user_friend_apply WHERE owner = ? AND apply = ?;");
	stmt.bind({username,target});
	stmt.step();
	stmt.prepare(db,"INSERT INTO user_friend (owner, friend, last_time, is_new) VALUES (?, ?, ?, ?);");
	stmt.bind({username,target,(long long)time(0),1});
	stmt.step();
	stmt.reset();
	stmt.bind({target,username,(long long)time(0),1});
	stmt.step();
	
	{
		lock_guard lock(mp_mtx);
		if(mp.count(target))
			mp[target]->send(json({{"type","accepted_apply"},{"target",username}}).dump());
	}
	return json({{"event",event_id},{"success",true}}).dump();
}

string handle_reject_friend_apply(const string &username,const string &target,const int &event_id)
{
	if(username==target)
		return json({{"event",event_id},{"success",false},{"error",u8"不能和自己加好友"}}).dump();
	SafeStmt stmt;
	stmt.prepare(db,"SELECT 1 FROM user_friend_apply WHERE owner = ? AND apply = ? LIMIT 1;");
	stmt.bind({username,target});
	if(stmt.step().first!=SQLITE_ROW)
		return json({{"event",event_id},{"success",false},{"error",u8"对方没有发送请求"}}).dump();
	stmt.prepare(db,"DELETE FROM user_friend_apply WHERE owner = ? AND apply = ?;");
	stmt.bind({username,target});
	stmt.step();
	
	{
		lock_guard lock(mp_mtx);
		if(mp.count(target))
			mp[target]->send(json({{"type","rejected_apply"},{"target",username}}).dump());
	}
	return json({{"event",event_id},{"success",true}}).dump();
}

string handle_get_message_count_group_main(const int &event_id)
{
	SafeStmt stmt;
	stmt.prepare(db,"SELECT COUNT(*) FROM group_main;");
	return json({{"event",event_id},{"type","message_count"},{"count",stmt.step().second[0].val_int}}).dump();
}
string handle_get_message_count_private(const string &chat_id,const string &username,const int &event_id)
{
	string target=chat_id.substr(8);
	SafeStmt stmt;
	stmt.prepare(db,"SELECT COUNT(*) FROM private_chat WHERE user_a = ? AND user_b = ?;");
	stmt.bind({min(target,username),max(target,username)});
	return json({{"event",event_id},{"type","message_count"},{"count",stmt.step().second[0].val_int}}).dump();
}
string handle_get_message_count(const string &chat_id,const string &username,const int &event_id)
{
	if(chat_id=="group_main")
		return handle_get_message_count_group_main(event_id);
	else if(chat_id.substr(0,7)=="private")
		return handle_get_message_count_private(chat_id,username,event_id);
	else
		return json({{"event",event_id},{"success",false},{"error",u8"错误的请求参数\"chat_id\""}}).dump();
}

void handle_ws(const Request &req,ws::WebSocket &ws)
{
	printf("%s[INFO]New WebSocket connect\n",getTime().c_str());
	string text,id="",username;
	while(ws.read(text)!=ws::Fail)
	{
		try {
			json js=nlohmann::json::parse(text);
			int event_id=(js.contains("event")?getInt(js,"event"):-1);
			string type=getStr(js,"type");
			if(!js.contains("type"))
				ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
			else if(type=="login")
			{
				if(handle_login(js))
				{
					username=getStr(js,"username");
					ws.send(json({{"event",event_id},{"success",true}}).dump());
					
					{
						lock_guard lock(mp_mtx);
						if(mp.count(username))
							mp[username]->close();
						mp[username]=&ws;
					}
				}
				else
					ws.send(json({{"event",event_id},{"success",false}}).dump());
			}
			else if(type=="register")
				ws.send(json({{"event",event_id},{"success",false},{"error",u8"注册入口已经关闭，请联系腐竹"}}).dump());
			else if(username.empty())
				ws.send(json({{"event",event_id},{"success",false},{"error",u8"未登录"}}).dump());
			else if(type=="send")
			{
				if(!js.contains("chat_id")||!js.contains("msg_type"))
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
					continue;
				}
				ws.send(handle_send_msg(js,username,event_id));
			}
			else if(type=="recall")
			{
				if(!js.contains("msg_id")||!js.contains("chat_id"))
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
					continue;
				}
				ws.send(handle_recall(js,username,event_id));
			}
			else if(type=="get_message_count")
			{
				if(!js.contains("chat_id"))
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
					continue;
				}
				ws.send(handle_get_message_count(getStr(js,"chat_id"),username,event_id));
			}
			else if(type=="get_history")
			{
				if(!js.contains("chat_id")||!js.contains("from")||!js.contains("to"))
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
					continue;
				}
				int from=getInt(js,"from"),to=getInt(js,"to");
				if(to-from+1>500)
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"请求消息数过多，单次仅限500条"}}).dump());
					continue;
				}
				ws.send(handle_get_history(from,to,getStr(js,"chat_id"),username,event_id));
			}
			else if(type=="get_friend_list")
				ws.send(handle_get_friend_list(username,event_id));
			else if(type=="get_friend_apply")
				ws.send(handle_get_friend_apply(username,event_id));
			else if(type=="add_friend")
			{
				if(!js.contains("target"))
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
					continue;
				}
				string target=getStr(js,"target");
				ws.send(handle_add_friend(username,target,event_id));
			}
			else if(type=="accept_friend")
			{
				if(!js.contains("target"))
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
					continue;
				}
				string target=getStr(js,"target");
				ws.send(handle_accept_friend_apply(username,target,event_id));
			}
			else if(type=="reject_friend")
			{
				if(!js.contains("target"))
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"不完整的请求参数"}}).dump());
					continue;
				}
				string target=getStr(js,"target");
				ws.send(handle_reject_friend_apply(username,target,event_id));
			}
			else if(type=="readed")
			{
				if(!js.contains("chat_id")||getStr(js,"chat_id")=="group_main"||getStr(js,"chat_id").substr(0,7)!="private")
				{
					ws.send(json({{"event",event_id},{"success",false},{"error",u8"错误的请求参数\"chat_id\""}}).dump());
					continue;
				}
				handle_tag_readed(username,getStr(js,"chat_id"));
				ws.send(json({{"event",event_id},{"success",true}}).dump());
			}
			else
				ws.send(json({{"event",event_id},{"success",false},{"error",u8"错误的请求参数\"type\""}}).dump());
		} catch(const exception &e) {
			ws.send(json({{"success",false},{"error","Server Error"}}).dump());
			printf("%s[ERROR]%s\n",getTime().c_str(),e.what());
			printf("%s\n",text.c_str());
		}
	}
	{
		lock_guard lock(mp_mtx);
		if(mp.count(username)&&mp[username]==&ws)
			mp.erase(username);
	}
	printf("%s[INFO]Disconnected!\n",getTime().c_str());
}

void init()
{
	SafeStmt stmt;
	stmt.prepare(db,
		 "CREATE TABLE IF NOT EXISTS user("
			 "username TEXT NOT NULL,"
			 "password TEXT NOT NULL"
		 ");");
	stmt.step();
	stmt.prepare(db,
		 "CREATE TABLE IF NOT EXISTS user_friend("
			 "owner TEXT NOT NULL,"
			 "friend TEXT NOT NULL,"
			 "last_time INT NOT NULL,"
			 "is_new INT NOT NULL,"
			 "PRIMARY KEY(owner, friend),"
			 "FOREIGN KEY(owner) REFERENCES user(username) ON DELETE CASCADE"
		 ");");
	stmt.step();
	stmt.prepare(db,
		 "CREATE TABLE IF NOT EXISTS user_friend_apply("
			 "owner TEXT NOT NULL,"
			 "apply TEXT NOT NULL,"
			 "PRIMARY KEY(owner, apply),"
			 "FOREIGN KEY(owner) REFERENCES user(username) ON DELETE CASCADE"
		 ");");
	stmt.step();
	stmt.prepare(db,
		 "CREATE TABLE IF NOT EXISTS private_chat("
			 "user_a TEXT NOT NULL,"
			 "user_b TEXT NOT NULL,"
			 "msg_id INT NOT NULL,"
			 "msg_type TEXT NOT NULL,"
			 "msg_sender TEXT NOT NULL,"
			 "msg_content TEXT NOT NULL,"
			 "msg_time INT NOT NULL,"
			 "PRIMARY KEY(user_a, user_b, msg_id)"
		 ");");
	stmt.step();
	stmt.prepare(db,
		 "CREATE TABLE IF NOT EXISTS group_main("
			 "msg_id INT PRIMARY KEY NOT NULL,"
			 "msg_type TEXT NOT NULL,"
			 "msg_sender TEXT NOT NULL,"
			 "msg_content TEXT NOT NULL,"
			 "msg_time INT NOT NULL"
		 ");");
	stmt.step();
}
#endif
