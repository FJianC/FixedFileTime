#include <iostream>
#include <Windows.h>
#include <direct.h>
#include <string>
#include <io.h>
#include <vector>
#include <time.h>

static int g_succ_count = 0;
static int g_fail_count = 0;
static std::vector<std::string> filter_vec = { ".", ".." };

bool FixedFileTime(const std::string& file_full_name, time_t set_time, const FILETIME& set_file_time) {
	static struct stat st;
	if (0 != stat(file_full_name.c_str(), &st)) {
		++g_fail_count;
		return false;
	}

	if (st.st_atime <= set_time && st.st_ctime <= set_time && st.st_mtime <= set_time)
		return false;

	static WCHAR w_file_name[1024];
	memset(w_file_name, 0, sizeof(w_file_name));

	MultiByteToWideChar(CP_ACP, 0, file_full_name.c_str(), (int)file_full_name.length(), w_file_name,
		sizeof(w_file_name) / sizeof(w_file_name[0]));

	const auto& h_file = CreateFileW(w_file_name, GENERIC_ALL,
		FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (INVALID_HANDLE_VALUE == h_file) {
		++g_fail_count;
		return false;
	}

	if (st.st_atime > set_time)
		SetFileTime(h_file, NULL, &set_file_time, NULL);
	if (st.st_ctime > set_time)
		SetFileTime(h_file, &set_file_time, NULL, NULL);
	if (st.st_mtime > set_time)
		SetFileTime(h_file, NULL, NULL, &set_file_time);

	++g_succ_count;

	CloseHandle(h_file);
	return true;
}

void RecursiveDir(const std::string& dir_path, time_t set_time, const FILETIME& set_file_time) {
	std::cout << "RecursiveDir: " << dir_path << std::endl;

	_finddata_t file_data;

	const auto& handle = _findfirst((dir_path + "\\*").c_str(), &file_data);
	if (-1L == handle) {
		++g_fail_count;
		return;
	}

	do {
		if (file_data.attrib & _A_SUBDIR) {
			if (filter_vec.end() == std::find(filter_vec.begin(), filter_vec.end(), file_data.name))
				RecursiveDir(dir_path + "\\" + file_data.name, set_time, set_file_time);
		}
		else {
			FixedFileTime(dir_path + "\\" + file_data.name, set_time, set_file_time);
		}
	} while (_findnext(handle, &file_data) == 0);

	_findclose(handle);

	FixedFileTime(dir_path, set_time, set_file_time);
}

void SplitString(const std::string& str, const std::string& delim) {
	const auto& delim_len = delim.length();
	std::string::size_type pos1 = 0, pos2 = str.find(delim);

	while (pos2 != std::string::npos) {
		filter_vec.push_back(std::string(str, pos1, pos2 - pos1));

		pos1 = pos2 + delim_len;
		pos2 = str.find(delim, pos1);
	}

	if (pos1 != str.length())
		filter_vec.push_back(std::string(str, pos1));
}

int main(int argc, char* argv[]) {
	std::cout << "Fixed File Time, If the time is greater than the current time" << std::endl;
	std::cout << "Enter a Path, using the current path if empty, for example: D:\\project" << std::endl;
	std::string dir_path;
	std::getline(std::cin, dir_path);
	std::cout << "Enter a Filter Dir, split with commas, for example: .svn,.git" << std::endl;
	std::string filter_str;
	std::getline(std::cin, filter_str);

	if (!filter_str.empty())
		SplitString(filter_str, ",");

	char tmp[1024] = { 0 };

	if (dir_path.empty()) {
		const auto& ret = _getcwd(tmp, sizeof(tmp) / sizeof(tmp[0]));
		dir_path = ret;
		std::cout << "path is empty, using current path: " << ret << std::endl;
	}

	const auto& set_time = std::time(nullptr);

	FILETIME set_file_time;
	{
		const LONGLONG& nLL = Int32x32To64(set_time, 10000000) + 116444736000000000;
		set_file_time.dwLowDateTime = (DWORD)nLL;
		set_file_time.dwHighDateTime = (DWORD)(nLL >> 32);
	}

	RecursiveDir(dir_path, set_time, set_file_time);

	ctime_s(tmp, sizeof(tmp) / sizeof(tmp[0]), &set_time);

	std::cout << "\nFixedFileTime path:" << dir_path << ", set_time:" << tmp << std::endl;
	std::cout << "SuccessCount:" << g_succ_count << std::endl;
	std::cout << "FailCount:" << g_fail_count << std::endl;

	return 0;
}
