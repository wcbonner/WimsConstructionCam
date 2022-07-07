#include <algorithm>
#include <arpa/inet.h>
#include <cfloat>
#include <climits>
#define _USE_MATH_DEFINES
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <locale>
#include <locale>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <queue>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h> // wait()
#include <unistd.h> // For close()
#include <utime.h>
#include <vector>
/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("WimConstructionCam Version 1.20220707-1 Built on: " __DATE__ " at " __TIME__);
int ConsoleVerbosity = 1;
int TimeoutMinutes = 0;
float Latitude = 47.670;
float Longitude = -122.382;
int GigabytesFreeSpace = 8;
/////////////////////////////////////////////////////////////////////////////
std::string timeToISO8601(const time_t& TheTime)
{
	std::ostringstream ISOTime;
	struct tm UTC;
	if (0 != gmtime_r(&TheTime, &UTC))
	{
		ISOTime.fill('0');
		if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
		{
			ISOTime << UTC.tm_year + 1900 << "-";
			ISOTime.width(2);
			ISOTime << UTC.tm_mon + 1 << "-";
			ISOTime.width(2);
			ISOTime << UTC.tm_mday << "T";
		}
		ISOTime.width(2);
		ISOTime << UTC.tm_hour << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_min << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_sec;
	}
	return(ISOTime.str());
}
std::string getTimeISO8601(void)
{
	time_t timer;
	time(&timer);
	std::string isostring(timeToISO8601(timer));
	std::string rval;
	rval.assign(isostring.begin(), isostring.end());

	return(rval);
}
time_t ISO8601totime(const std::string& ISOTime)
{
	struct tm UTC;
	UTC.tm_year = stol(ISOTime.substr(0, 4)) - 1900;
	UTC.tm_mon = stol(ISOTime.substr(5, 2)) - 1;
	UTC.tm_mday = stol(ISOTime.substr(8, 2));
	UTC.tm_hour = stol(ISOTime.substr(11, 2));
	UTC.tm_min = stol(ISOTime.substr(14, 2));
	UTC.tm_sec = stol(ISOTime.substr(17, 2));
	UTC.tm_gmtoff = 0;
	UTC.tm_isdst = -1;
	UTC.tm_zone = 0;
#ifdef _MSC_VER
	_tzset();
	_get_daylight(&(UTC.tm_isdst));
#endif
# ifdef __USE_MISC
	time_t timer = timegm(&UTC);
#else
	time_t timer = mktime(&UTC);
	timer -= timezone; // HACK: Works in my initial testing on the raspberry pi, but it's currently not DST
#endif
#ifdef _MSC_VER
	long Timezone_seconds = 0;
	_get_timezone(&Timezone_seconds);
	timer -= Timezone_seconds;
	int DST_hours = 0;
	_get_daylight(&DST_hours);
	long DST_seconds = 0;
	_get_dstbias(&DST_seconds);
	timer += DST_hours * DST_seconds;
#else
#endif
	return(timer);
}
// Microsoft Excel doesn't recognize ISO8601 format dates with the "T" seperating the date and time
// This function puts a space where the T goes for ISO8601. The dates can be decoded with ISO8601totime()
std::string timeToExcelDate(const time_t& TheTime)
{
	std::ostringstream ExcelDate;
	struct tm UTC;
	if (0 != gmtime_r(&TheTime, &UTC))
	{
		ExcelDate.fill('0');
		ExcelDate << UTC.tm_year + 1900 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mon + 1 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mday << " ";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_hour << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_min << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_sec;
	}
	return(ExcelDate.str());
}
std::string timeToExcelLocal(const time_t& TheTime)
{
	std::ostringstream ExcelDate;
	struct tm UTC;
	if (0 != localtime_r(&TheTime, &UTC))
	{
		ExcelDate.fill('0');
		ExcelDate << UTC.tm_year + 1900 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mon + 1 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mday << " ";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_hour << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_min << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_sec;
	}
	return(ExcelDate.str());
}
/////////////////////////////////////////////////////////////////////////////
double radians(const double degrees)
{
	return((degrees * M_PI) / 180.0);
}
double degrees(const double radians)
{
	return((radians * 180.0) / M_PI);
}
#ifdef DAYLLIGHT
bool isDayTime(const time_t& TheTime, const double SunDeclin = 47.670, double Longitude = -122.382)
{
	struct tm LocalTime;
	if (0 != localtime_r(&TheTime, &LocalTime))
	{
		double JulianDay = D2 + 2415018.5 + E2 - $B$5 / 24;
		double JulianCentury = (JulianDay - 2451545) / 36525;
		double MeanObliqEcliptic = 23 + (26 + ((21.448 - G2 * (46.815 + G2 * (0.00059 - G2 * 0.001813)))) / 60) / 60;
		double ObliqCorr = MeanObliqEcliptic + 0.00256 * cos(radians(125.04 - 1934.136 * JulianCentury));
		double var_y = tan(radians(R2 / 2)) * tan(radians(R2 / 2));
		double EquationOfTime = 4 * degrees(var_y * sin(2 * radians(GeomMeanLongSun)) - 2 * EccentEarthOrbit * sin(radians(GeomMeanAnomSun)) + 4 * EccentEarthOrbit * var_y * sin(radians(GeomMeanAnomSun)) * sin(2 * radians(GeomMeanLongSun)) - 0.5 * var_y * var_y * sin(4 * radians(GeomMeanLongSun)) - 1.25 * EccentEarthOrbit * EccentEarthOrbit * sin(2 * radians(GeomMeanAnomSun)));
		double HASunriseDeg = degrees(acos(cos(radians(90.833)) / (cos(radians(Latitude)) * cos(radians(SunDeclin))) - tan(radians(Latitude)) * tan(radians(SunDeclin))));
		double SolarNoon = (720 - 4 * Longitude - EquationOfTime + LocalTime.tm_gmtoff / 60) / 1440;
		double SunriseTime = SolarNoon - HASunriseDeg * 4 / 1440;
		double SunsetTime = SolarNoon + HASunriseDeg * 4 / 1440;
	}
}
#endif
/////////////////////////////////////////////////////////////////////////////
bool ValidateDirectory(std::string& DirectoryName)
{
	bool rval = false;
	// I want to make sure the directory name does not end with a "/"
	while (DirectoryName.back() == '/')
		DirectoryName.erase(DirectoryName.back());
	//TODO: I want to make sure the directory exists
	struct statvfs buffer2;
	if (0 == statvfs(DirectoryName.c_str(), &buffer2))
		rval = true;
	//TODO: I want to make sure the directory is writable by the current user
	return(rval);
}
/////////////////////////////////////////////////////////////////////////////
std::string GetImageDirectory(const std::string DestinationDir, const time_t& TheTime)
{
	// returns valid image directory name without trailing "/"
	std::ostringstream OutputDirectoryName;
	OutputDirectoryName << DestinationDir << "/";
	struct tm UTC;
	if (0 != localtime_r(&TheTime, &UTC))
	{
		OutputDirectoryName.fill('0');
		OutputDirectoryName << UTC.tm_year + 1900;
		OutputDirectoryName.width(2);
		OutputDirectoryName << UTC.tm_mon + 1;
		OutputDirectoryName.width(2);
		OutputDirectoryName << UTC.tm_mday;
	}

	struct statvfs buffer;
	if (0 != statvfs(OutputDirectoryName.str().c_str(), &buffer))
		//if (!(buffer.st_mode & _S_IFDIR))
	{
		if (0 == mkdir(OutputDirectoryName.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		{
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601() << "] Directory Created: " << OutputDirectoryName.str() << std::endl;
			else
				std::cerr << "Directory Created : " << OutputDirectoryName.str() << std::endl;
		}
	}
	return(OutputDirectoryName.str());
}
int GetLastImageNum(const std::string DestinationDir)
{
	int LastImageNum = 0;
	struct statvfs buffer2;
	if (0 == statvfs(DestinationDir.c_str(), &buffer2))
	{
		DIR* dp;
		if ((dp = opendir(DestinationDir.c_str())) != NULL)
		{
			std::deque<std::string> files;
			struct dirent* dirp;
			while ((dirp = readdir(dp)) != NULL)
				if (DT_REG == dirp->d_type)
					files.push_back(std::string(dirp->d_name));
			closedir(dp);
			if (!files.empty())
			{
				sort(files.begin(), files.end());
				std::string LastFile(files.back());
				while (LastFile.find(".mp4") != std::string::npos)
				{
					files.pop_back();
					LastFile = files.back();
				}
				LastImageNum = atoi(LastFile.substr(4, 4).c_str());
			}
		}
	}
	return(LastImageNum);
}
/////////////////////////////////////////////////////////////////////////////
void GenerateFreeSpace(const int MinFreeSpaceGB, const std::string DestinationDir)
{
	unsigned long long MinFreeSpace = (unsigned long long)(MinFreeSpaceGB) << 30ll;
	std::ostringstream OutputDirectoryName;
	OutputDirectoryName << DestinationDir;
	struct statvfs buffer2;
	if (0 == statvfs(OutputDirectoryName.str().c_str(), &buffer2))
	{
		if (ConsoleVerbosity > 0)
		{
			std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " optimal transfer block size: " << buffer2.f_bsize << std::endl;
			std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " total data blocks in file system: " << buffer2.f_blocks << std::endl;
			std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " free blocks in fs: " << buffer2.f_bfree << std::endl;
			std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " free blocks avail to non-superuser: " << buffer2.f_bavail << std::endl;
			std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " Drive Size: " << buffer2.f_bsize * buffer2.f_blocks << " Free Space: " << buffer2.f_bsize * buffer2.f_bavail << std::endl;
		}
		DIR* dp;
		if ((dp = opendir(OutputDirectoryName.str().c_str())) != NULL)
		{
			std::deque<std::string> files;
			std::deque<std::string> directories;
			struct dirent* dirp;
			while ((dirp = readdir(dp)) != NULL)
				if (DT_REG == dirp->d_type)
				{
					std::string filename = OutputDirectoryName.str() + "/" + std::string(dirp->d_name);
					files.push_back(filename);
				}
				else if (DT_DIR == dirp->d_type)
				{
					if (!std::string(dirp->d_name).compare("..") && !std::string(dirp->d_name).compare("."))
					{
						std::string dirname = OutputDirectoryName.str() + "/" + std::string(dirp->d_name);
						directories.push_back(dirname);
					}
				}
			closedir(dp);
			if (!files.empty())
				sort(files.begin(), files.end());
			//for (std::map<time_t, string>::const_iterator filename = files.begin(); filename != files.end(); filename++)
			//	cout << "[" << timeToISO8601(LogFileTime) << "] " << filename->second << endl;
			while ((!files.empty()) && (buffer2.f_bsize * buffer2.f_bavail < MinFreeSpace))	// This loop will make sure that there's free space on the drive.
			{
				struct stat buffer;
				if (0 == stat(files.begin()->c_str(), &buffer))
					if (0 == remove(files.begin()->c_str()))
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601() << "] File Deleted: " << *files.begin() << "(" << buffer.st_size << ")" << std::endl;
				//cout << "[" << timeToISO8601(LogFileTime) << "] " << dirp->d_name << " st_ctime: " << buffer.st_ctime << endl;
				//cout << "[" << timeToISO8601(LogFileTime) << "] " << dirp->d_name << " st_mtime: " << buffer.st_mtime << endl;
				//cout << "[" << timeToISO8601(LogFileTime) << "] " << dirp->d_name << " st_atime: " << buffer.st_atime << endl;
				//files[buffer.st_mtime] = dirp->d_name;
				files.pop_front();
				if (0 != statvfs(OutputDirectoryName.str().c_str(), &buffer2))
					break;
				//for (std::vector<string>::const_iterator filename = files.begin(); filename != files.end(); filename++)
				//	cout << "[" << timeToISO8601(LogFileTime) << "] " << *filename << endl;
			}
			if (!directories.empty())
				sort(directories.begin(), directories.end());
			while ((!directories.empty()) && (buffer2.f_bsize * buffer2.f_bavail < MinFreeSpace))	// This loop will make sure that there's free space on the drive.
			{
				GenerateFreeSpace(MinFreeSpaceGB, *directories.begin());
				directories.pop_front();
				if (0 != statvfs(OutputDirectoryName.str().c_str(), &buffer2))
					break;
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
bool CreateDailyMovie(const std::string DailyDirectory)
{
	bool rval = false;
	DIR* dp;
	if ((dp = opendir(DailyDirectory.c_str())) != NULL)
	{
		std::deque<std::string> JPGfiles;
		std::deque<std::string> MP4files;
		struct dirent* dirp;
		while ((dirp = readdir(dp)) != NULL)
			if (DT_REG == dirp->d_type)
			{
				std::string filename = DailyDirectory + "/" + std::string(dirp->d_name);
				if (filename.find(".jpg") != std::string::npos)
					JPGfiles.push_back(filename);
				else if (filename.find(".mp4") != std::string::npos)
					MP4files.push_back(filename);
			}
		closedir(dp);
		if (MP4files.empty())
		{
			if (!JPGfiles.empty())
				sort(JPGfiles.begin(), JPGfiles.end());
			std::string LastLPGFile(JPGfiles.back());
			struct stat buffer;
			if (0 == stat(LastLPGFile.c_str(), &buffer))
			{
				struct tm UTC;
				if (0 != localtime_r(&buffer.st_mtim.tv_sec, &UTC))
				{
					std::ostringstream StillFormat;	// raspistill outputname format string
					StillFormat.fill('0');
					StillFormat << DailyDirectory << "/";
					StillFormat.width(2);
					StillFormat << UTC.tm_mon + 1;
					StillFormat.width(2);
					StillFormat << UTC.tm_mday;
					StillFormat << "\%04d.jpg";

					std::ostringstream VideoFileName;	// ffmpeg output video name
					VideoFileName.fill('0');
					VideoFileName << DailyDirectory << "/";
					VideoFileName.width(4);
					VideoFileName << UTC.tm_year + 1900;
					VideoFileName.width(2);
					VideoFileName << UTC.tm_mon + 1;
					VideoFileName.width(2);
					VideoFileName << UTC.tm_mday;
					char MyHostName[HOST_NAME_MAX] = { 0 }; // hostname used for data recordkeeping
					if (gethostname(MyHostName, sizeof(MyHostName)) == 0)
						VideoFileName << "-" << MyHostName;
					VideoFileName << ".mp4";
					if (ConsoleVerbosity > 0)
					{
						std::cout << "[" << getTimeISO8601() << "]   StillFormat: " << StillFormat.str() << std::endl;
						std::cout << "[" << getTimeISO8601() << "] VideoFileName: " << VideoFileName.str() << std::endl;
					}
					else
					{
						std::cerr << "   StillFormat: " << StillFormat.str() << std::endl;
						std::cerr << " VideoFileName: " << VideoFileName.str() << std::endl;
					}
					pid_t pid_FFMPEG = fork();
					if (pid_FFMPEG == 0)
					{
						/* A zero PID indicates that this is the child process */
						/* Replace the child fork with a new process */
						if (execlp("ffmpeg", "ffmpeg",
							"-hide_banner",
							"-r", "30",
							"-i", StillFormat.str().c_str(),
							"-vf", "drawtext=font=sans:fontcolor=white:fontsize=60:y=main_h-text_h-30:x=main_w-text_w-30:text=WimsConstructionCam,drawtext=font=mono:fontcolor=white:fontsize=60:y=main_h-text_h-30:x=30:text=%{metadata\\\\:DateTimeOriginal}",
							"-c:v", "libx265",
							"-crf", "23",
							"-preset", "veryfast",
							"-movflags", "+faststart",
							"-bf", "2",
							"-g", "15",
							"-pix_fmt", "yuv420p",
							"-n",
							VideoFileName.str().c_str(),
							NULL) == -1)
						{
							std::cerr << "execlp Error! ffmpeg." << std::endl;
						}
					}
					else if (pid_FFMPEG > 0)
					{
						/* A positive (non-negative) PID indicates the parent process */
						int ffmpeg_exit_status = 0;
						wait(&ffmpeg_exit_status);				/* Wait for child process to end */
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601() << "] ffmpeg exited with a  " << ffmpeg_exit_status << " value" << std::endl;
						else if (ffmpeg_exit_status != 0)
							std::cerr << "ffmpeg exited with a  " << ffmpeg_exit_status << " value" << std::endl;
						else
							rval = true;
					}
					else
					{
						std::cerr << "Fork error! ffmpeg." << std::endl;  /* something went wrong */
					}
				}
			}
		}
	}
	return(rval);
}
void CreateAllDailyMovies(const std::string DestinationDir)
{
	DIR* dp;
	if ((dp = opendir(DestinationDir.c_str())) != NULL)
	{
		std::deque<std::string> Subdirectories;
		struct dirent* dirp;
		while ((dirp = readdir(dp)) != NULL)
			if (DT_DIR == dirp->d_type)
			{
				std::string DirectoryName(dirp->d_name);
				if ((DirectoryName.compare("..") == 0) || (DirectoryName.compare(".") == 0))
					continue;
				else
				{
					std::string dirname = DestinationDir + "/" + std::string(dirp->d_name);
					Subdirectories.push_back(dirname);
				}
			}
		closedir(dp);
		while (!Subdirectories.empty())
		{
			CreateDailyMovie(Subdirectories.front());
			Subdirectories.pop_front();
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
volatile bool bRun = true; // This is declared volatile so that the compiler won't optimized it out of loops later in the code
void SignalHandlerSIGINT(int signal)
{
	bRun = false;
	std::cerr << "***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************" << std::endl;
}
void SignalHandlerSIGHUP(int signal)
{
	bRun = false;
	std::cerr << "***************** SIGHUP: Caught HangUp, finishing loop and quitting. *****************" << std::endl;
}
/////////////////////////////////////////////////////////////////////////////
std::string DestinationDir("/home/wim/DCIM");
static void usage(int argc, char** argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  " << ProgramVersionString << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -h | --help          Print this message" << std::endl;
	std::cout << "    -v | --verbose level stdout verbosity level [" << ConsoleVerbosity << "]" << std::endl;
	std::cout << "    -d | --destination location pictures will be stored [" << DestinationDir << "]" << std::endl;
	std::cout << "    -f | --freespace gigabytes free space per day [" << GigabytesFreeSpace << "]" << std::endl;
	std::cout << "    -t | --time minutes of stills to capture [" << TimeoutMinutes << "]" << std::endl;
	std::cout << "    -l | --lat latitude for sunrise/sunset [" << Latitude << "]" << std::endl;
	std::cout << "    -L | --lon longitude for sunrise/sunset [" << Longitude << "]" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hv:d:f:t:l:L:";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "destination",	required_argument, NULL, 'd' },
		{ "freespace",required_argument, NULL, 'f' },
		{ "time",required_argument, NULL, 't' },
		{ "lat",required_argument, NULL, 't' },
		{ "lon",required_argument, NULL, 't' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////
// Here's some web pages related to sunrise/sunset calculations since there's no reason to be taking pictures in the dark.
// https://gml.noaa.gov/grad/solcalc/calcdetails.html
// https://gml.noaa.gov/grad/solcalc/
// https://gml.noaa.gov/grad/solcalc/sunrise.html
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (ConsoleVerbosity > 0)
	{
		std::cout << "[" << getTimeISO8601() << "] " << ProgramVersionString << std::endl;
	}
	else
		std::cerr << ProgramVersionString << " (starting)" << std::endl;
	///////////////////////////////////////////////////////////////////////////////////////////////
	tzset();
	///////////////////////////////////////////////////////////////////////////////////////////////
	for (;;)
	{
		int idx;
		int c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c)
		{
		case 0: /* getopt_long() flag */
			break;
		case 'h':
			usage(argc, argv);
			exit(EXIT_SUCCESS);
		case 'v':
			try { ConsoleVerbosity = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'd':
			DestinationDir = std::string(optarg);
			break;
		case 'f':
			try { GigabytesFreeSpace = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 't':
			try { TimeoutMinutes = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'l':
			try { Latitude = std::stof(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'L':
			try { Longitude = std::stof(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	CreateAllDailyMovies(DestinationDir);
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Set up CTR-C signal handler
	typedef void (*SignalHandlerPointer)(int);
	SignalHandlerPointer previousHandler = signal(SIGINT, SignalHandlerSIGINT);
	bRun = true;
	while (bRun)
	{
		time_t LoopStartTime;
		time(&LoopStartTime);
		// largest file in sample was 1,310,523, multiply by minutes in day 1440, 1887153120, round up to 2000000000 or 2GB.
		GenerateFreeSpace(GigabytesFreeSpace, DestinationDir);
		std::string ImageDirectory(GetImageDirectory(DestinationDir, LoopStartTime));
		std::ostringstream OutputFormat;	// raspistill outputname format string
		std::ostringstream FrameStart;		// first filename for raspistill to use in current loop
		std::ostringstream Timeout;			// how many milliseconds raspistill will run
		// Minutes in Day = 60 * 24 = 1440
		int MinutesLeftInDay = 1440;
		struct tm UTC;
		if (0 != localtime_r(&LoopStartTime, &UTC))
		{
			int CurrentMinuteInDay = UTC.tm_hour * 60 + UTC.tm_min;
			MinutesLeftInDay = 1440 - CurrentMinuteInDay;
			if (TimeoutMinutes == 0)
				Timeout << MinutesLeftInDay * 60 * 1000;
			else 
				Timeout << TimeoutMinutes * 60 * 1000;
			OutputFormat.fill('0');
			OutputFormat << ImageDirectory << "/";
			OutputFormat.width(2);
			OutputFormat << UTC.tm_mon + 1;
			OutputFormat.width(2);
			OutputFormat << UTC.tm_mday;
			OutputFormat << "\%04d.jpg";
			FrameStart << GetLastImageNum(ImageDirectory) + 1;
		}
		else
			bRun = false;

		if (ConsoleVerbosity > 0)
		{
			std::cout << "[" << getTimeISO8601() << "]  OutputFormat: " << OutputFormat.str() << std::endl;
			std::cout << "[" << getTimeISO8601() << "]    FrameStart: " << FrameStart.str() << std::endl;
			std::cout << "[" << getTimeISO8601() << "]       Timeout: " << Timeout.str() << std::endl;
		}

		if (bRun)
		{
			/* Attempt to fork */
			pid_t pid = fork();
			if (pid == 0)
			{
				/* A zero PID indicates that this is the child process */
				/* Replace the child fork with a new process */

				std::string CameraProgram("raspistill");

				if (ConsoleVerbosity > 0)
				{
					std::cout << "[" << getTimeISO8601() << "]        execlp: ";
					std::cout << CameraProgram << " ";
					std::cout << "--nopreview" << " ";
					std::cout << "--thumb" << " " << "none" << " ";
					std::cout << "--width" << " " << "1920" << " ";
					std::cout << "--height" << " " << "1080" << " ";
					std::cout << "--timeout" << " " << Timeout.str() << " ";
					std::cout << "--timelapse" << " " << "60000" << " ";
					std::cout << "--output" << " " << OutputFormat.str() << " ";
					std::cout << "--framestart" << " " << FrameStart.str() << " ";
					std::cout << std::endl;
				}
				// https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/raspicam/RaspiStill.c
				// raspistill should exit with a 0 (EX_OK) on success, or 70 (EX_SOFTWARE)
				if (execlp(CameraProgram.c_str(), CameraProgram.c_str(),
					"--nopreview", 
					"--thumb", "none", 
					"--width", "1920", 
					"--height", "1080", 
					"--timeout", Timeout.str().c_str(), 
					"--timelapse", "60000", 
					"--output", OutputFormat.str().c_str(), 
					"--framestart", FrameStart.str().c_str(), 
					NULL) == -1)
				{
					std::cerr << "[" << getTimeISO8601() << "] execlp Error! " << CameraProgram << std::endl;
					CameraProgram = "libcamera-still";
					if (ConsoleVerbosity > 0)
					{
						std::cout << "[" << getTimeISO8601() << "]  execlp: ";
						std::cout << CameraProgram << " ";
						std::cout << "--nopreview" << " ";
						std::cout << "--thumb" << " " << "none" << " ";
						std::cout << "--width" << " " << "1920" << " ";
						std::cout << "--height" << " " << "1080" << " ";
						std::cout << "--timeout" << " " << Timeout.str() << " ";
						std::cout << "--timelapse" << " " << "60000" << " ";
						std::cout << "--output" << " " << OutputFormat.str() << " ";
						std::cout << "--framestart" << " " << FrameStart.str() << " ";
						std::cout << std::endl;
					}
					// https://github.com/raspberrypi/libcamera-apps/blob/main/apps/libcamera_still.cpp
					// libcamera-still exits with a 0 on success, or -1 if it catches an exception.
					if (execlp(CameraProgram.c_str(), CameraProgram.c_str(),
						"--nopreview",
						"--thumb", "none",
						"--width", "1920",
						"--height", "1080",
						"--timeout", Timeout.str().c_str(),
						"--timelapse", "60000",
						"--output", OutputFormat.str().c_str(),
						"--framestart", FrameStart.str().c_str(),
						NULL) == -1)
					{
						std::cerr << "[" << getTimeISO8601() << "] execlp Error! " << CameraProgram << std::endl;
						bRun = false;
					}
				}
			}
			else if (pid > 0)
			{
				/* A positive (non-negative) PID indicates the parent process */
				int CameraProgram_exit_status = 0;
				wait(&CameraProgram_exit_status);				/* Wait for child process to end */
				if (CameraProgram_exit_status != 0)
					std::cerr << "[" << getTimeISO8601() << "] CameraProgram exited with a  " << CameraProgram_exit_status << " value" << std::endl;
				else if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] CameraProgram exited with a  " << CameraProgram_exit_status << " value" << std::endl;
			}
			else
			{
				std::cerr << "[" << getTimeISO8601() << "] Fork error! CameraProgram." << std::endl;  /* something went wrong */
				bRun = false;
			}
			if (bRun)
				bRun = CreateDailyMovie(ImageDirectory);
		}
	}
	// remove our special Ctrl-C signal handler and restore previous one
	signal(SIGINT, previousHandler);

	///////////////////////////////////////////////////////////////////////////////////////////////
	std::cerr << ProgramVersionString << " (exiting)" << std::endl;
	return(EXIT_SUCCESS);
}