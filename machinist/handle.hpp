 
#ifndef MACHINIST_HANDLE__
#define MACHINIST_HANDLE__

// sort of using this as a platform file
#include <exception>
#include <memory>
#include <string>
#include <vector>

using std::vector;
using std::string;
using std::shared_ptr;

#define REQUIRE(cond, msg) if (cond); else throw std::runtime_error(string(msg).c_str());
#define THROW(msg) throw std::runtime_error(string(msg).c_str());
template<class T_> T_ Next(const T_& p) { return p + 1; }
template<class T_> const T_& Max(const T_& a, const T_& b) { return a > b ? a : b; }
template<class T_> const T_& Min(const T_& a, const T_& b) { return a < b ? a : b; }

template<class T_> class Handle_ : public shared_ptr<const T_>
{
public:
	Handle_() : shared_ptr<const T_>() {}
	Handle_(const T_* p) : shared_ptr<const T_>(p) {}
	Handle_(const shared_ptr<const T_>& src) : shared_ptr<const T_>(src) {}
};

string EnvironmentValue(const string& name);

#endif

