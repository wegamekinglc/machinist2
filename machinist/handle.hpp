#pragma once

// sort of using this as a platform file
#include <memory>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER
    constexpr char OS_SEP = '\\';
#else
    constexpr char OS_SEP = '/';
#endif

struct A_ {
    A_(const char* fn) {
        std::cout << "Initializing " << fn << "\n";
    }
};

#define HERE                                \
    namespace {                             \
        static const A_ theA(__FILE__);     \
    }


#define REQUIRE(cond, msg)                                                                                             \
    if (cond)                                                                                                          \
        ;                                                                                                              \
    else                                                                                                               \
        throw std::runtime_error(std::string(msg).c_str())
#define THROW(msg) throw std::runtime_error(std::string(msg).c_str());
template <class T_> T_ Next(const T_& p) { return p + 1; }
template <class T_> const T_& Max(const T_& a, const T_& b) { return a > b ? a : b; }
template <class T_> const T_& Min(const T_& a, const T_& b) { return a < b ? a : b; }

template <class T_> class Handle_ : public std::shared_ptr<const T_> {
public:
    Handle_() : std::shared_ptr<const T_>() {}
    Handle_(const T_* p) : std::shared_ptr<const T_>(p) {}
    Handle_(const std::shared_ptr<const T_>& src) : std::shared_ptr<const T_>(src) {}
};

std::string EnvironmentValue(const std::string& name);