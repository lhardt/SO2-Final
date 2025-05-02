#include "utils.hpp"

#include <string>
#include <cctype>

std::string get_first_word(const std::string &s, int & last_index_ret){
	std::string result;
	int i = 0;
	for(; i < s.size(); ++i){
		char ch = s[i];
		if( !std::isspace((unsigned char) ch )){
			result += std::tolower(ch);
		} else if( result.size() > 0 ){
			break;
		}
	}
	last_index_ret = i;
	return result;
}

std::string trim_string(const std::string &s){
	std::string result = "";

	// i_start points to the first valid character, and similarly for i_end
	int i_start = 0;
	int i_end = s.size() - 1;

	while(i_start < s.size() && std::isspace(s[i_start])) ++i_start;
	while(i_end >= 0 && std::isspace(s[i_end])) --i_end;

	if(i_end < 0 || i_start >= s.size() || i_start > i_end ){
		return "";
	} 

	return s.substr(i_start, i_end-i_start+1); 
}
