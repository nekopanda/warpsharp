#include <vector>
#include <list>
#include <map>
#include <set>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <windows.h>

#include "common/node.h"
#include "common/library.h"

#include "platform/vfw.h"

#define IMPLEMENT
#include "platform/console.h"
#undef IMPLEMENT

#include "avisynth/avisynth.h"

#include "aviutl/filter.h"

#include "singleton.h"
#include "aviutl.h"
#include "loadauf.h"
#include "loadauf2.h"

//////////////////////////////////////////////////////////////////////////////

static void Usage(std::ostream& os, const AviUtlFilterPluginRef& plugin) {
	std::vector<std::string> type, name, value, comment;
	const FILTER *filter = plugin->GetFilter();

	for(int i = 0; i < filter->track_n; ++i) {
		type.push_back("int");

		std::ostringstream os;
		os << "i" << i;
		name.push_back(os.str());

		os.rdbuf()->str("");
		os << filter->track_default[i];
		value.push_back(os.str());

		os.rdbuf()->str("");
		os << filter->track_name[i] << " default(";
		os << filter->track_default[i] << ") range(";
		os << filter->track_s[i] << ",";
		os << filter->track_e[i] << ")";
		comment.push_back(os.str());
	}

	for(int i = 0; i < filter->check_n; ++i) {
		type.push_back("bool");

		std::ostringstream os;
		os << std::boolalpha << "b" << i;
		name.push_back(os.str());

		os.rdbuf()->str("");
		os << bool(filter->check_default[i]);
		value.push_back(os.str());

		os.rdbuf()->str("");
		os << filter->check_name[i] << " default(";
		os << value.back() << ")";
		comment.push_back(os.str());
	}

	char full[MAX_PATH], *module;
	GetFullPathName(plugin->GetLibrary()->GetPath(), sizeof(full), full, &module);

	std::string func("AU_");
	func += module;
	int extpos = func.find('.');

	if(extpos != std::string::npos)
		func.erase(extpos);

	os << "####################################" << std::endl;
	os << "# " << filter->name << std::endl;
	os << "####################################" << std::endl;

	int size = type.size();

	for(int i = 0; i < size; ++i)
		os << "# " << name[i] << " : " << comment[i] << std::endl;

	os << "function " << func << "(clip clip";

	for(int i = 0; i < size; ++i) {
		os << ",";

		if(i % 4 == 0) {
			os << std::endl;
			os << "  \\";
		}

		os << " " << type[i] << " \"" << name[i] << "\"";
	}

	os << ")" << std::endl;
	os << "{" << std::endl;
	os << "  " << LoadAviUtlFilterPlugin::GetName() << "(AviUtl_plugin_directory+\"";
	os << module <<  "\", \"_" << func << "\", copy=AviUtl_plugin_copy, debug=AviUtl_plugin_debug, thread=AviUtl_plugin_thread)" << std::endl;
	os << "  #" << LoadAviUtlFilterPlugin2::GetName() << "(AviUtl_plugin_directory+\"";
	os << module <<  "\", \"_" << func << "\", copy=AviUtl_plugin_copy, debug=AviUtl_plugin_debug, thread=AviUtl_plugin_thread)" << std::endl;
	os << "  return clip._" << func << "(";

	for(int i = 0; i < size; ++i) {
		if(i != 0)
			os << ",";

		if(i % 4 == 0) {
			os << std::endl;
			os << "    \\";
		}

		os << " default(" << name[i] << "," << value[i] << ")";
	}

	os << ")" << std::endl;
	os << "}" << std::endl;
	os << "# example:" << std::endl;
	os << "# " << ConvertYUY2ToAviUtlYC::GetName() << "()" << std::endl;
	os << "# " << func << "(";

	for(int i = 0; i < size; ++i) {
		if(i != 0) os << ",";
		os << value[i];
	}

	os << ")" << std::endl;
	os << "# " << ConvertAviUtlYCToYUY2::GetName() << "()" << std::endl;
	os << std::endl;
}

//////////////////////////////////////////////////////////////////////////////

void main(int argc, char *argv[]) {
	std::ostream& os = std::cout;

	os << "global AviUtl_plugin_directory = \"C:\\AviUtl\\Plugins\\\"" << std::endl;
	os << "global AviUtl_plugin_copy = false" << std::endl;
	os << "global AviUtl_plugin_debug = false" << std::endl;
	os << "global AviUtl_plugin_thread = 2" << std::endl;
	os << std::endl;

	for(int i = 1; i < argc; ++i) {
		AviUtlFilterPluginRef plugin(new AviUtlFilterPlugin(argv[i], "", false, false, 2));
		if(!!plugin) Usage(os, plugin);
	}
}
