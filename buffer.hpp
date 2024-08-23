#pragma once
#include <vector>
#include <iostream>
#include<algorithm>
#include <assert.h>
#include<signal.h>
#include<string.h>
#include "logger.hpp"
#define BUFFER_DEFAULT_SIZE 1024
class Buffer
{
private:
    std::vector<char> _buffer; // 储存
    uint64_t _reader_idx;      // 读写偏移量
    uint64_t _writer_idx;

public:
    Buffer() : _reader_idx(0), _writer_idx(0), _buffer(BUFFER_DEFAULT_SIZE) {}
    char *buffer_begin() { return &_buffer[0]; }                     // buffer起始地址
    char *write_position() { return buffer_begin() + _writer_idx; }; // 写起始
    char *read_position() { return buffer_begin() + _reader_idx; };  // 读起始
    uint64_t taild_size() { return _buffer.size() - _writer_idx; }   // 尾空余
    uint64_t head_size() { return _reader_idx; }                     // 头空余
    uint64_t readable_size() { return _writer_idx - _reader_idx; }
    void move_read_offset(uint64_t len) // readidx向后移动len
    {
        if (len <= 0)
            return;
        assert(len <= readable_size());
        _reader_idx += len;
    }
    void move_write_offect(uint64_t len) // writeidx向后移动len
    {
        assert(len <= taild_size());
        _writer_idx += len;
    }
    // 确保可写空间足够,移动or扩容
    void ensure_write_space(uint64_t len)
    {
        if (taild_size() >= len)
            return;
        if (len <= taild_size() + head_size())
        {
            uint64_t old_size = readable_size();
            std::copy(read_position(), read_position() + old_size, buffer_begin());
            _reader_idx = 0;
            _writer_idx = old_size;
            return;
        }
        // 扩容数据
        DLOG("RESIZE %ld", _writer_idx + len);
        _buffer.resize(_writer_idx + len);
    }
    // 普通写入数据接口
    void write_normal(const void *data, uint64_t len)
    {
        if (len == 0)
            return;
        ensure_write_space(len);
        const char *tmp = (const char *)data;
        std::copy(tmp, tmp + len, write_position());
    }
    void write_normal_push(const void *data, uint64_t len)
    {
        write_normal(data, len);
        move_write_offect(len);
    }
    // 写入string接口
    void write_string(const std::string &data)
    {
        return write_normal(data.c_str(), data.size()); // string底层字符串直接操纵
    }
    void write_string_push(const std::string &data)
    {
        write_string(data); // 也可以直接调用 wri normal psh
        move_write_offect(data.size());
    }
    // 写入buffer,也可以构造函数
    void write_by_buffer(Buffer &data) // 用const会获取普通指针出错
    {
        // std::cout<< data.read_position()-data.buffer_begin() <<std::endl;
        // std::cout<< data.readable_size() <<std::endl;
        return write_normal(data.read_position(), data.readable_size());
    }
    void write_buffer_push(Buffer &data)
    {
        //std::cout<< "进入" <<std::endl;
        write_by_buffer(data);
        move_write_offect(data.readable_size());
    }
    // 读取数据
    void read_normal(void *buf, uint64_t len)
    {
        assert(len <= readable_size());
        std::copy(read_position(), read_position() + len, (char*)buf);
        //
    }
    void read_normal_pop(void *buf, uint64_t len)
    {
        read_normal(buf, len);
        move_read_offset(len);
    }
    std::string read_asstring(uint64_t len)
    {
        assert(len <= readable_size());
        std::string res;
        res.resize(len);
        read_normal(&res[0], len);
        return res;
    }
    std::string read_asstring_pop(uint64_t len)
    {
        std::string res = read_asstring(len);
        move_read_offset(len);
        return res;
    }
     
    char *find_CRLF()
    {
        char *begin = read_position();
        char *end = begin + readable_size();
        char *res = std::find(begin, end,'\n');
        return (res != end) ? res : nullptr;
    }
std::string get_line()
{
    char*pos=find_CRLF();
    if(pos==nullptr)
    return "";
    return read_asstring(pos-read_position()+1);//把换行符也提取出来
}
std::string get_line_pop()
{
    std::string res=get_line();
    move_read_offset(res.size());
    return res;
}
void clear()
{
    _reader_idx=0;
    _writer_idx=0;
}
   
};
