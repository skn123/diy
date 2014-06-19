#ifndef DIY_IO_NMPY_HPP
#define DIY_IO_NMPY_HPP

#include <stdexcept>
#include "bov.hpp"

namespace diy
{
namespace io
{
  class NumPyReader: public BOVReader
  {
    public:
      typedef       BOVReader::Shape                Shape;
    public:
                    NumPyReader(std::ifstream& in):
                      BOVReader(in)
      {
        Shape  shape;
        bool   fortran;
        size_t offset = parse_npy_header(shape, fortran);
        BOVReader::set_offset(offset);
        BOVReader::set_shape(shape);
      }

      unsigned          word_size() const                       { return word_size_; }

    private:
      inline size_t     parse_npy_header(Shape& shape, bool& fortran_order);

    private:
      unsigned          word_size_;
  };
}
}

// Modified from: https://github.com/rogersce/cnpy
// Copyright (C) 2011  Carl Rogers
// Released under MIT License
// license available at http://www.opensource.org/licenses/mit-license.php
size_t
diy::io::NumPyReader::
parse_npy_header(Shape& shape, bool& fortran_order)
{
    char buffer[256];
    in().read(buffer,11);
    if(!in())
        throw std::runtime_error("parse_npy_header: failed fread");
    std::string header;
    std::getline(in(), header);
    size_t header_size = header.size() + 1 + 11;

    int loc1, loc2;

    //fortran order
    loc1 = header.find("fortran_order")+16;
    fortran_order = (header.substr(loc1,5) == "True" ? true : false);

    //shape
    unsigned ndims;
    loc1 = header.find("(");
    loc2 = header.find(")");
    std::string str_shape = header.substr(loc1+1,loc2-loc1-1);
    if(str_shape[str_shape.size()-1] == ',') ndims = 1;
    else ndims = std::count(str_shape.begin(),str_shape.end(),',')+1;
    shape.resize(ndims);
    for(unsigned int i = 0;i < ndims;i++) {
        loc1 = str_shape.find(",");
        shape[i] = atoi(str_shape.substr(0,loc1).c_str());
        str_shape = str_shape.substr(loc1+1);
    }

    //endian, word size, data type
    //byte order code | stands for not applicable.
    //not sure when this applies except for byte array
    loc1 = header.find("descr")+9;
    bool littleEndian = (header[loc1] == '<' || header[loc1] == '|' ? true : false);
    //assert(littleEndian);

    //char type = header[loc1+1];
    //assert(type == map_type(T));

    std::string str_ws = header.substr(loc1+2);
    loc2 = str_ws.find("'");
    word_size_ = atoi(str_ws.substr(0,loc2).c_str());

    return header_size;
}

#endif