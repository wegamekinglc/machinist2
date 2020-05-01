 
#ifndef MACHINIST_EMITTER__
#define MACHINIST_EMITTER__

#ifndef MACHINIST_HANDLE__
#include "Handle.h"
#endif

#include <string>
#include <map>

using std::map;
struct Info_;
namespace Emitter
{
	struct Funcs_;
   vector<string> Call(const Info_& arg, const Funcs_& lib, const string& which);
   vector<string> CallTransform(const string& src, const Funcs_& lib, const string& which);
}

// Emitter_ structure takes an info and produces code

class Emitter_
{
public:
	virtual ~Emitter_();

	virtual vector<string> operator()
		(const Info_& arg, const Emitter::Funcs_& lib)
	const = 0;
};

class StringTransform_
{
public:
	virtual ~StringTransform_();

	virtual vector<string> operator()
		(const string& src, const Emitter::Funcs_& lib)
	const = 0;
};



namespace Emitter
{
	struct Funcs_
	{
		map<string, Handle_<Emitter_>> ofInfo_;
		map<string, Handle_<StringTransform_>> ofString_;
		bool empty() const { return ofInfo_.empty(); }
		void clear() { ofInfo_.clear(); ofString_.clear(); }
	};

	struct Source_
	{
		virtual ~Source_();
		virtual Funcs_ Parse
			(const vector<string>& lib,	// contents of library, not paths
			 const string& path) 
		const = 0;
	};
	void RegisterSource
		(const string& info_type,
		 const Source_& src);

	const Funcs_& GetAll
		(const string& info_type,
		 const string& template_path,	// might parse a template file, lazily; will hold a static registry
		 const vector<string>& library_contents);
}

#endif

