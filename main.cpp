#include <iostream>
#include <mysql/mysql.h>
#include <json/json.h>
#include "cnf.h"
#include "httplib.h"
#include <fstream>

#define DATA_SIZE 100
const std::string videoFilePath = "/";

void writeToResponse(httplib::Response& res,const char* buffer,size_t size){
	res.body.append(buffer,size);
}

void sendVideo(const httplib::Request& req, httplib::Response& res) {
    std::ifstream videoFile(videoFilePath, std::ios::binary);
    if (!videoFile) {
        res.status = 404;
        res.set_content("Video file not found", "text/plain");
        return;
    }

    videoFile.seekg(0, std::ios::end);
    size_t fileSize = videoFile.tellg();
    videoFile.seekg(0, std::ios::beg);

    res.set_header("Content-Type", "video/mp4");
    res.set_header("Content-Length", std::to_string(fileSize));
    res.set_header("Cache-Control", "no-cache");  // Prevent caching issues

    std::string rangeHeader = req.get_header_value("Range");
    if (!rangeHeader.empty()) {
        size_t startPos = rangeHeader.find("=") + 1;
        size_t dashPos = rangeHeader.find("-");
        size_t start = std::stoull(rangeHeader.substr(startPos, dashPos - startPos));

        size_t end = fileSize - 1;
        if (dashPos != std::string::npos && dashPos + 1 < rangeHeader.size()) {
            end = std::stoull(rangeHeader.substr(dashPos + 1));
        }

        if (start >= fileSize) {
            res.status = 416; 
            res.set_content("Requested range is not satisfiable", "text/plain");
            return;
        }

        res.status = 206;
        res.set_header("Content-Range", "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(fileSize));
        res.set_header("Content-Length", std::to_string(end - start + 1));

        videoFile.seekg(start, std::ios::beg);
        size_t bufferSize = 1024 * 16; 
        char buffer[bufferSize];

        size_t bytesToSend = end - start + 1;
        while (bytesToSend > 0) {
            size_t chunkSize = std::min(bytesToSend, bufferSize);
            videoFile.read(buffer, chunkSize);
            writeToResponse(res, buffer, chunkSize);
            bytesToSend -= chunkSize;
        }
    } else {
        res.set_header("Content-Range", "bytes 0-" + std::to_string(fileSize - 1) + "/" + std::to_string(fileSize)); // For compatibility
        size_t bufferSize = 1024 * 1024;
        char buffer[bufferSize];
        while (videoFile.read(buffer, bufferSize)) {
            writeToResponse(res, buffer, bufferSize);
        }
        size_t remaining = videoFile.gcount();
        if (remaining > 0) {
            writeToResponse(res, buffer, remaining);
        }
    }

    videoFile.close();
}


Json::Value get_user_data(){
	Json::Value result;
	MYSQL *conn;
	MYSQL_RES *res;
	MYSQL_ROW row;
	conn = mysql_init(NULL);
	if(!mysql_real_connect(conn,host,user,pwd,db,port,NULL,0)){
		Json::Value error_json;
		error_json["status"] = "error";
		error_json["message"] = "Error connecting to database:" + std::string(mysql_error(conn));
		return error_json;
	}
	if(mysql_query(conn,"SELECT name,description,price FROM goods.`goods`;")){
		std::cerr<<"error query database:"<<mysql_error(conn)<<std::endl;
		Json::Value error_json;
		error_json["status"] = "error";
		error_json["message"] = "Error querying database:" + std::string(mysql_error(conn));
		return error_json;
	}
	res = mysql_use_result(conn);
	Json::Value users(Json::arrayValue);
	while((row = mysql_fetch_row(res)) != NULL){
		Json::Value user;
		std::vector<std::string> columns = {
			"name", "description", "price"
			};
		for (size_t i = 0; i < columns.size(); ++i) {
			user[columns[i]] = row[i] ? row[i] : "";
		}
		users.append(user);
	}
	result["status"] = "success";
	result["data"] = users;
	std::cout << "Generated JSON: " << result.toStyledString() << std::endl;
	mysql_free_result(res);
	mysql_close(conn);
	return result;
}

void generate_price(double *price,int size){

}

int main(){
	double price[DATA_SIZE];
	httplib::Server svr;
	svr.Get("/getUserData",[](const httplib::Request& req,httplib::Response& res){
			Json::Value data = get_user_data();
			res.set_content(data.toStyledString(),"application/json");
	});
	svr.Get("/getVideo",sendVideo); 
	std::cout<<"Server started at http://localhost:8080"<<std::endl;
	svr.listen("0.0.0.0",8080);
	return 0;
}
