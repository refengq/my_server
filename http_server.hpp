#include"tcpserver.hpp"
#include <fstream>
#include <regex>
#include <sys/stat.h>

#define DEFALT_TIMEOUT 10

std::unordered_map<int, std::string> _statu_msg = {
    {100,  "Continue"},
    {101,  "Switching Protocol"},
    {102,  "Processing"},
    {103,  "Early Hints"},
    {200,  "OK"},
    {201,  "Created"},
    {202,  "Accepted"},
    {203,  "Non-Authoritative Information"},
    {204,  "No Content"},
    {205,  "Reset Content"},
    {206,  "Partial Content"},
    {207,  "Multi-Status"},
    {208,  "Already Reported"},
    {226,  "IM Used"},
    {300,  "Multiple Choice"},
    {301,  "Moved Permanently"},
    {302,  "Found"},
    {303,  "See Other"},
    {304,  "Not Modified"},
    {305,  "Use Proxy"},
    {306,  "unused"},
    {307,  "Temporary Redirect"},
    {308,  "Permanent Redirect"},
    {400,  "Bad Request"},
    {401,  "Unauthorized"},
    {402,  "Payment Required"},
    {403,  "Forbidden"},
    {404,  "Not Found"},
    {405,  "Method Not Allowed"},
    {406,  "Not Acceptable"},
    {407,  "Proxy Authentication Required"},
    {408,  "Request Timeout"},
    {409,  "Conflict"},
    {410,  "Gone"},
    {411,  "Length Required"},
    {412,  "Precondition Failed"},
    {413,  "Payload Too Large"},
    {414,  "URI Too Long"},
    {415,  "Unsupported Media Type"},
    {416,  "Range Not Satisfiable"},
    {417,  "Expectation Failed"},
    {418,  "I'm a teapot"},
    {421,  "Misdirected Request"},
    {422,  "Unprocessable Entity"},
    {423,  "Locked"},
    {424,  "Failed Dependency"},
    {425,  "Too Early"},
    {426,  "Upgrade Required"},
    {428,  "Precondition Required"},
    {429,  "Too Many Requests"},
    {431,  "Request Header Fields Too Large"},
    {451,  "Unavailable For Legal Reasons"},
    {501,  "Not Implemented"},
    {502,  "Bad Gateway"},
    {503,  "Service Unavailable"},
    {504,  "Gateway Timeout"},
    {505,  "HTTP Version Not Supported"},
    {506,  "Variant Also Negotiates"},
    {507,  "Insufficient Storage"},
    {508,  "Loop Detected"},
    {510,  "Not Extended"},
    {511,  "Network Authentication Required"}
};

std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac",        "audio/aac"},
    {".abw",        "application/x-abiword"},
    {".arc",        "application/x-freearc"},
    {".avi",        "video/x-msvideo"},
    {".azw",        "application/vnd.amazon.ebook"},
    {".bin",        "application/octet-stream"},
    {".bmp",        "image/bmp"},
    {".bz",         "application/x-bzip"},
    {".bz2",        "application/x-bzip2"},
    {".csh",        "application/x-csh"},
    {".css",        "text/css"},
    {".csv",        "text/csv"},
    {".doc",        "application/msword"},
    {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot",        "application/vnd.ms-fontobject"},
    {".epub",       "application/epub+zip"},
    {".gif",        "image/gif"},
    {".htm",        "text/html"},
    {".html",       "text/html"},
    {".ico",        "image/vnd.microsoft.icon"},
    {".ics",        "text/calendar"},
    {".jar",        "application/java-archive"},
    {".jpeg",       "image/jpeg"},
    {".jpg",        "image/jpeg"},
    {".js",         "text/javascript"},
    {".json",       "application/json"},
    {".jsonld",     "application/ld+json"},
    {".mid",        "audio/midi"},
    {".midi",       "audio/x-midi"},
    {".mjs",        "text/javascript"},
    {".mp3",        "audio/mpeg"},
    {".mpeg",       "video/mpeg"},
    {".mpkg",       "application/vnd.apple.installer+xml"},
    {".odp",        "application/vnd.oasis.opendocument.presentation"},
    {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt",        "application/vnd.oasis.opendocument.text"},
    {".oga",        "audio/ogg"},
    {".ogv",        "video/ogg"},
    {".ogx",        "application/ogg"},
    {".otf",        "font/otf"},
    {".png",        "image/png"},
    {".pdf",        "application/pdf"},
    {".ppt",        "application/vnd.ms-powerpoint"},
    {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar",        "application/x-rar-compressed"},
    {".rtf",        "application/rtf"},
    {".sh",         "application/x-sh"},
    {".svg",        "image/svg+xml"},
    {".swf",        "application/x-shockwave-flash"},
    {".tar",        "application/x-tar"},
    {".tif",        "image/tiff"},
    {".tiff",       "image/tiff"},
    {".ttf",        "font/ttf"},
    {".txt",        "text/plain"},
    {".vsd",        "application/vnd.visio"},
    {".wav",        "audio/wav"},
    {".weba",       "audio/webm"},
    {".webm",       "video/webm"},
    {".webp",       "image/webp"},
    {".woff",       "font/woff"},
    {".woff2",      "font/woff2"},
    {".xhtml",      "application/xhtml+xml"},
    {".xls",        "application/vnd.ms-excel"},
    {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml",        "application/xml"},
    {".xul",        "application/vnd.mozilla.xul+xml"},
    {".zip",        "application/zip"},
    {".3gp",        "video/3gpp"},
    {".3g2",        "video/3gpp2"},
    {".7z",         "application/x-7z-compressed"}
};

class util {
public:
    static size_t split(const std::string &src, const std::string &sep, std::vector<std::string> *arry) {
        size_t offset = 0;
        while (offset < src.size()) {
            size_t pos = src.find(sep, offset);
            if (pos == std::string::npos) {
                if (pos == src.size()) break;
                arry->push_back(src.substr(offset));
                return arry->size();
            }
            if (pos == offset) {
                offset = pos + sep.size();
                continue;
            }
            arry->push_back(src.substr(offset, pos - offset));
            offset = pos + sep.size();
        }
        return arry->size();
    }

    static bool read_file(const std::string &filename, std::string *buf) {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs.is_open()) {
            printf("OPEN %s FILE FAILED!!", filename.c_str());
            return false;
        }
        size_t fsize = 0;
        ifs.seekg(0, ifs.end);
        fsize = ifs.tellg();
        ifs.seekg(0, ifs.beg);
        buf->resize(fsize);
        ifs.read(&(*buf)[0], fsize);
        if (!ifs.good()) {
            printf("READ %s FILE FAILED!!", filename.c_str());
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }

    static bool write_file(const std::string &filename, const std::string &buf) {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            printf("OPEN %s FILE FAILED!!", filename.c_str());
            return false;
        }
        ofs.write(buf.c_str(), buf.size());
        if (!ofs.good()) {
            printf("WRITE %s FILE FAILED!", filename.c_str());
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }

    static std::string url_encode(const std::string &url, bool convert_space_to_plus) {
        std::string res;
        for (auto &c : url) {
            if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c)) {
                res += c;
                continue;
            }
            if (c == ' ' && convert_space_to_plus) {
                res += '+';
                continue;
            }
            char tmp[4] = {0};
            snprintf(tmp, 4, "%%%02X", c);
            res += tmp;
        }
        return res;
    }

    static char hextoi(char c) {
        if (c >= '0' && c <= '9') {
            return c - '0';
        } else if (c >= 'a' && c <= 'z') {
            return c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z') {
            return c - 'A' + 10;
        }
        return -1;
    }

    static std::string url_decode(const std::string &url, bool convert_plus_to_space) {
        std::string res;
        for (int i = 0; i < url.size(); i++) {
            if (url[i] == '+' && convert_plus_to_space) {
                res += ' ';
                continue;
            }
            if (url[i] == '%' && (i + 2) < url.size()) {
                char v1 = hextoi(url[i + 1]);
                char v2 = hextoi(url[i + 2]);
                char v = v1 * 16 + v2;
                res += v;
                i += 2;
                continue;
            }
            res += url[i];
        }
        return res;
    }

    static std::string statu_desc(int statu) {
        auto it = _statu_msg.find(statu);
        if (it != _statu_msg.end()) {
            return it->second;
        }
        return "Unknow";
    }

    static std::string ext_mime(const std::string &filename) {
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos) {
            return "application/octet-stream";
        }
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end()) {
            return "application/octet-stream";
        }
        return it->second;
    }

    static bool is_directory(const std::string &filename) {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0) {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }

    static bool is_regular(const std::string &filename) {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0) {
            return false;
        }
        return S_ISREG(st.st_mode);
    }

    static bool valid_path(const std::string &path) {
        std::vector<std::string> subdir;
        split(path, "/", &subdir);
        int level = 0;
        for (auto &dir : subdir) {
            if (dir == "..") {
                level--;
                if (level < 0) return false;
                continue;
            }
            level++;
        }
        return true;
    }
};

class http_request {
public:
    std::string _method;
    std::string _path;
    std::string _version;
    std::string _body;
    std::smatch _matches;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _params;

public:
    http_request() : _version("HTTP/1.1") {}

    void reset() {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _body.clear();
        std::smatch match;
        _matches.swap(match);
        _headers.clear();
        _params.clear();
    }

    void set_header(const std::string &key, const std::string &val) {
        _headers.insert(std::make_pair(key, val));
    }

    bool has_header(const std::string &key) const {
        auto it = _headers.find(key);
        if (it == _headers.end()) {
            return false;
        }
        return true;
    }

    std::string get_header(const std::string &key) const {
        auto it = _headers.find(key);
        if (it == _headers.end()) {
            return "";
        }
        return it->second;
    }

    void set_param(const std::string &key, const std::string &val) {
        _params.insert(std::make_pair(key, val));
    }

    bool has_param(const std::string &key) const {
        auto it = _params.find(key);
        if (it == _params.end()) {
            return false;
        }
        return true;
    }

    std::string get_param(const std::string &key) const {
        auto it = _params.find(key);
        if (it == _params.end()) {
            return "";
        }
        return it->second;
    }

    size_t content_length() const {
        bool ret = has_header("Content-Length");
        if (!ret) {
            return 0;
        }
        std::string clen = get_header("Content-Length");
        return std::stol(clen);
    }

    bool close() const {
        if (has_header("Connection") && get_header("Connection") == "keep-alive") {
            return false;
        }
        return true;
    }
};

class http_response {
public:
    int _statu;
    bool _redirect_flag;
    std::string _body;
    std::string _redirect_url;
    std::unordered_map<std::string, std::string> _headers;

public:
    http_response() : _redirect_flag(false), _statu(200) {}

    http_response(int statu) : _redirect_flag(false), _statu(statu) {}

    void reset() {
        _statu = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }

    void set_header(const std::string &key, const std::string &val) {
        _headers.insert(std::make_pair(key, val));
    }

    bool has_header(const std::string &key) {
        auto it = _headers.find(key);
        if (it == _headers.end()) {
            return false;
        }
        return true;
    }

    std::string get_header(const std::string &key) {
        auto it = _headers.find(key);
        if (it == _headers.end()) {
            return "";
        }
        return it->second;
    }

    void set_content(const std::string &body, const std::string &type = "text/html") {
        _body = body;
        set_header("Content-Type", type);
    }

    void set_redirect(const std::string &url, int statu = 302) {
        _statu = statu;
        _redirect_flag = true;
        _redirect_url = url;
    }

    bool close() {
        if (has_header("Connection") && get_header("Connection") == "keep-alive") {
            return
        }