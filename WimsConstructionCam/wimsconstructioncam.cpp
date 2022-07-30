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
#include <sysexits.h>
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
#ifdef _USE_GPSD
#include <gps.h>        // apt install libgps-dev
#include <libgpsmm.h>   // apt install libgps-dev
#endif

// GPSD Client HOWTO https://gpsd.io/client-howto.html#_c_examples
// https://www.ubuntupit.com/best-gps-tools-for-linux/
// https://www.linuxlinks.com/GPSTools/
/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("WimsConstructionCam 1.20220730-1 Built " __DATE__ " at " __TIME__);
int ConsoleVerbosity = 1;
int TimeoutMinutes = 0;
bool UseGPSD = false;
double Latitude = 0;
double Longitude = 0;
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
std::string getTimeExcelLocal(void)
{
	time_t timer;
	time(&timer);
	std::string isostring(timeToExcelLocal(timer));
	std::string rval;
	rval.assign(isostring.begin(), isostring.end());
	return(rval);
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
double Time2JulianDate(const time_t& TheTime)
{
	double JulianDay = 0;
	struct tm UTC;
	if (0 != gmtime_r(&TheTime, &UTC))
	{
		// https://en.wikipedia.org/wiki/Julian_day
		// JDN = (1461 × (Y + 4800 + (M ? 14)/12))/4 +(367 × (M ? 2 ? 12 × ((M ? 14)/12)))/12 ? (3 × ((Y + 4900 + (M - 14)/12)/100))/4 + D ? 32075
		JulianDay = (1461 * ((UTC.tm_year + 1900) + 4800 + ((UTC.tm_mon + 1) - 14) / 12)) / 4
			+ (367 * ((UTC.tm_mon + 1) - 2 - 12 * (((UTC.tm_mon + 1) - 14) / 12))) / 12
			- (3 * (((UTC.tm_year + 1900) + 4900 + ((UTC.tm_mon + 1) - 14) / 12) / 100)) / 4
			+ (UTC.tm_mday)
			- 32075;
		// JD = JDN + (hour-12)/24 + minute/1440 + second/86400
		double partialday = (static_cast<double>((UTC.tm_hour - 12)) / 24) + (static_cast<double>(UTC.tm_min) / 1440.0) + (static_cast<double>(UTC.tm_sec) / 86400.0);
		JulianDay += partialday;
	}
	return(JulianDay);
}
time_t JulianDate2Time(const double JulianDate)
{
	time_t TheTime = (JulianDate - 2440587.5) * 86400;
	return(TheTime);
}
double JulianDate2JulianDay(const double JulianDate)
{
	double n = JulianDate - 2451545.0 + 0.0008;
	return(n);
}
/////////////////////////////////////////////////////////////////////////////
// These equations all come from https://en.wikipedia.org/wiki/Sunrise_equation
double getMeanSolarTime(const double JulianDay, const double longitude)
{
	// an approximation of mean solar time expressed as a Julian day with the day fraction.
	double MeanSolarTime = JulianDay - (longitude / 360);
	return (MeanSolarTime);
}
double getSolarMeanAnomaly(const double MeanSolarTime)
{
	double SolarMeanAnomaly = fmod(357.5291 + 0.98560028 * MeanSolarTime, 360);
	return(SolarMeanAnomaly);
}
double getEquationOfTheCenter(const double SolarMeanAnomaly)
{
	double EquationOfTheCenter = 1.9148 * sin(radians(SolarMeanAnomaly)) + 0.0200 * sin(radians(2 * SolarMeanAnomaly)) + 0.0003 * sin(radians(3 * SolarMeanAnomaly));
	return(EquationOfTheCenter);
}
double getEclipticLongitude(const double SolarMeanAnomaly, const double EquationOfTheCenter)
{
	double EclipticLongitude = fmod(SolarMeanAnomaly + EquationOfTheCenter + 180 + 102.9372, 360);
	return(EclipticLongitude);
}
double getSolarTransit(const double MeanSolarTime, const double SolarMeanAnomaly, const double EclipticLongitude)
{
	// the Julian date for the local true solar transit (or solar noon).
	double SolarTransit = 2451545.0 + MeanSolarTime + 0.0053 * sin(radians(SolarMeanAnomaly)) - 0.0069 * sin(radians(2 * EclipticLongitude));
	return(SolarTransit);
}
double getDeclinationOfTheSun(const double EclipticLongitude)
{
	double DeclinationOfTheSun = sin(radians(EclipticLongitude)) * sin(radians(23.44));
	return(DeclinationOfTheSun);
}
double getHourAngle(const double Latitude, const double DeclinationOfTheSun)
{
	double HourAngle = (sin(radians(-0.83)) - sin(radians(Latitude)) * sin(radians(DeclinationOfTheSun))) / (cos(radians(Latitude)) * cos(radians(DeclinationOfTheSun)));
	return(HourAngle);
}
double getSunrise(const double SolarTransit, const double HourAngle)
{
	double Sunrise = SolarTransit - (HourAngle / 360);
	return(Sunrise);
}
double getSunset(const double SolarTransit, const double HourAngle)
{
	double Sunset = SolarTransit + (HourAngle / 360);
	return(Sunset);
}
/////////////////////////////////////////////////////////////////////////////
// From NOAA Spreadsheet https://gml.noaa.gov/grad/solcalc/calcdetails.html
bool getSunriseSunset(time_t& Sunrise, time_t& Sunset, const time_t& TheTime, const double Latitude, double Longitude)
{
	bool rval = false;
	struct tm LocalTime;
	if (0 != localtime_r(&TheTime, &LocalTime))
	{
		// if we don't have a valid latitude or longitude, declare sunrise to be midnight, and sunset one second before midnight
		if ((Latitude == 0) || (Longitude == 0))
		{
			LocalTime.tm_hour = 0;
			LocalTime.tm_min = 0;
			LocalTime.tm_sec = 0;
			Sunrise = mktime(&LocalTime);
			Sunset = Sunrise + 24*60*60 - 1;
		}
		else
		{
			double JulianDay = Time2JulianDate(TheTime); // F
			double JulianCentury = (JulianDay - 2451545) / 36525;	// G
			double GeomMeanLongSun = fmod(280.46646 + JulianCentury * (36000.76983 + JulianCentury * 0.0003032), 360);	// I
			double GeomMeanAnomSun = 357.52911 + JulianCentury * (35999.05029 - 0.0001537 * JulianCentury);	// J
			double EccentEarthOrbit = 0.016708634 - JulianCentury * (0.000042037 + 0.0000001267 * JulianCentury);	// K
			double SunEqOfCtr = sin(radians(GeomMeanAnomSun)) * (1.914602 - JulianCentury * (0.004817 + 0.000014 * JulianCentury)) + sin(radians(2 * GeomMeanAnomSun)) * (0.019993 - 0.000101 * JulianCentury) + sin(radians(3 * GeomMeanAnomSun)) * 0.000289; // L
			double SunTrueLong = GeomMeanLongSun + SunEqOfCtr;	// M
			double SunAppLong = SunTrueLong - 0.00569 - 0.00478 * sin(radians(125.04 - 1934.136 * JulianCentury));	// P
			double MeanObliqEcliptic = 23 + (26 + ((21.448 - JulianCentury * (46.815 + JulianCentury * (0.00059 - JulianCentury * 0.001813)))) / 60) / 60;	// Q
			double ObliqCorr = MeanObliqEcliptic + 0.00256 * cos(radians(125.04 - 1934.136 * JulianCentury));	// R
			double SunDeclin = degrees(asin(sin(radians(ObliqCorr)) * sin(radians(SunAppLong))));	// T
			double var_y = tan(radians(ObliqCorr / 2)) * tan(radians(ObliqCorr / 2));	// U
			double EquationOfTime = 4 * degrees(var_y * sin(2 * radians(GeomMeanLongSun)) - 2 * EccentEarthOrbit * sin(radians(GeomMeanAnomSun)) + 4 * EccentEarthOrbit * var_y * sin(radians(GeomMeanAnomSun)) * sin(2 * radians(GeomMeanLongSun)) - 0.5 * var_y * var_y * sin(4 * radians(GeomMeanLongSun)) - 1.25 * EccentEarthOrbit * EccentEarthOrbit * sin(2 * radians(GeomMeanAnomSun))); // V
			double HASunriseDeg = degrees(acos(cos(radians(90.833)) / (cos(radians(Latitude)) * cos(radians(SunDeclin))) - tan(radians(Latitude)) * tan(radians(SunDeclin)))); // W
			double SolarNoon = (720 - 4 * Longitude - EquationOfTime + LocalTime.tm_gmtoff / 60) / 1440; // X
			double SunriseTime = SolarNoon - HASunriseDeg * 4 / 1440;	// Y
			double SunsetTime = SolarNoon + HASunriseDeg * 4 / 1440;	// Z
			LocalTime.tm_hour = 0;
			LocalTime.tm_min = 0;
			LocalTime.tm_sec = 0;
			time_t Midnight = mktime(&LocalTime);
			Sunrise = Midnight + SunriseTime * 86400;
			Sunset = Midnight + SunsetTime * 86400;
		}
		rval = true;
	}
	return(rval);
}
/////////////////////////////////////////////////////////////////////////////
bool getLatLon(double& Latitude, double& Longitude)
{
	bool rval = false;
#ifdef _USE_GPSD
	gpsmm gps_rec("localhost", DEFAULT_GPSD_PORT);
	if (gps_rec.stream(WATCH_ENABLE | WATCH_JSON) == NULL)
		std::cerr << "No GPSD running." << std::endl;
	else
	{
#if GPSD_API_MAJOR_VERSION < 9
		timestamp_t last_timestamp = 0;
#else
		timespec_t last_timestamp;
		timespec_get(&last_timestamp, TIME_UTC);
#endif
		int doloop = 0;
		while (doloop < 5)
		{
			struct gps_data_t* newdata;
			if (!gps_rec.waiting(1000000)) // wait 1 second, time is in microseconds
			{
				doloop++;
				continue;
			}
			if ((newdata = gps_rec.read()) == NULL)
			{
				std::cerr << "GPSD read error." << std::endl;
				doloop += 10;
			}
			else
			{
				if (newdata->set & MODE_SET)
					if ((newdata->fix.mode > 2) && (newdata->set & LATLON_SET) && (newdata->set & TIME_SET))
					{
						Latitude = newdata->fix.latitude;
						Longitude = newdata->fix.longitude;
						rval = true;
						if (ConsoleVerbosity > 0)
						{
							//std::cout << "[" << getTimeExcelLocal() << "]  Fix Time: " << timeToISO8601(newdata->fix.time) << std::endl;
							std::cout << "[" << getTimeExcelLocal() << "]  Latitude: " << std::setprecision(std::numeric_limits<double>::max_digits10) << newdata->fix.latitude << std::endl;
							std::cout << "[" << getTimeExcelLocal() << "] Longitude: " << std::setprecision(std::numeric_limits<double>::max_digits10) << newdata->fix.longitude << std::endl;
						}
						doloop+=10;
					}
			}
		}
	}
#endif
	return(rval);
}
/////////////////////////////////////////////////////////////////////////////
bool ValidateDirectory(std::string& DirectoryName)
{
	bool rval = false;
	// I want to make sure the directory name does not end with a "/"
	while ((!DirectoryName.empty()) && (DirectoryName.back() == '/'))
		DirectoryName.erase(DirectoryName.back());
	// https://linux.die.net/man/2/stat
	struct stat StatBuffer;
	if (0 == stat(DirectoryName.c_str(), &StatBuffer))
		if (S_ISDIR(StatBuffer.st_mode))
		{
#define USE_ACCESS
#ifndef USE_ACCESS
			std::ostringstream TemporaryFilename;
			TemporaryFilename << DirectoryName << "/WimsCam.tmp";
			std::ofstream TheTemporaryFile(TemporaryFilename.str());
			if (TheTemporaryFile.is_open())
			{
				TheTemporaryFile.close();
				remove(TemporaryFilename.str().c_str());
				rval = true;
			}
#else
			// https://linux.die.net/man/2/access
			if (0 == access(DirectoryName.c_str(), R_OK | W_OK))
				rval = true;
			else
			{
				switch (errno)
				{
				case EACCES:
					std::cerr << DirectoryName << " (" << errno << ") The requested access would be denied to the file, or search permission is denied for one of the directories in the path prefix of pathname." << std::endl;
					break;
				case ELOOP:
					std::cerr << DirectoryName << " (" << errno << ") Too many symbolic links were encountered in resolving pathname." << std::endl;
					break;
				case ENAMETOOLONG:
					std::cerr << DirectoryName << " (" << errno << ") pathname is too long." << std::endl;
					break;
				case ENOENT:
					std::cerr << DirectoryName << " (" << errno << ") A component of pathname does not exist or is a dangling symbolic link." << std::endl;
					break;
				case ENOTDIR:
					std::cerr << DirectoryName << " (" << errno << ") A component used as a directory in pathname is not, in fact, a directory." << std::endl;
					break;
				case EROFS:
					std::cerr << DirectoryName << " (" << errno << ") Write permission was requested for a file on a read-only file system." << std::endl;
					break;
				case EFAULT:
					std::cerr << DirectoryName << " (" << errno << ") pathname points outside your accessible address space." << std::endl;
					break;
				case EINVAL:
					std::cerr << DirectoryName << " (" << errno << ") mode was incorrectly specified." << std::endl;
					break;
				case EIO:
					std::cerr << DirectoryName << " (" << errno << ") An I/O error occurred." << std::endl;
					break;
				case ENOMEM:
					std::cerr << DirectoryName << " (" << errno << ") Insufficient kernel memory was available." << std::endl;
					break;
				case ETXTBSY:
					std::cerr << DirectoryName << " (" << errno << ") Write access was requested to an executable which is being executed." << std::endl;
					break;
				default:
					std::cerr << DirectoryName << " (" << errno << ") An unknown error." << std::endl;
				}
			}
#endif // USE_ACCESS
		}
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
				std::cout << "[" << getTimeExcelLocal() << "] Directory Created: " << OutputDirectoryName.str() << std::endl;
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
			std::cout << "[" << getTimeExcelLocal() << "] " << OutputDirectoryName.str() << " optimal transfer block size: " << buffer2.f_bsize << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "] " << OutputDirectoryName.str() << " total data blocks in file system: " << buffer2.f_blocks << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "] " << OutputDirectoryName.str() << " free blocks in fs: " << buffer2.f_bfree << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "] " << OutputDirectoryName.str() << " free blocks avail to non-superuser: " << buffer2.f_bavail << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "] " << OutputDirectoryName.str() << " Drive Size: " << buffer2.f_bsize * buffer2.f_blocks << " Free Space: " << buffer2.f_bsize * buffer2.f_bavail << std::endl;
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
							std::cout << "[" << getTimeExcelLocal() << "] File Deleted: " << *files.begin() << "(" << buffer.st_size << ")" << std::endl;
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
bool CreateDailyStills(const std::string DestinationDir, const time_t& TheTime, const time_t& Sunset)
{
	bool rval = false;
	std::ostringstream OutputFormat;	// raspistill outputname format string
	std::ostringstream FrameStart;		// first filename for raspistill to use in current loop
	std::ostringstream Timeout;			// how many milliseconds raspistill will run
	// Minutes in Day = 60 * 24 = 1440
	int MinutesLeftInDay = 1440;
	struct tm UTC;
	if (0 != localtime_r(&TheTime, &UTC))
	{
		int CurrentMinuteInDay = UTC.tm_hour * 60 + UTC.tm_min;
		struct tm SunsetTM;
		if (0 != localtime_r(&Sunset, &SunsetTM))
			MinutesLeftInDay = (SunsetTM.tm_hour * 60 + SunsetTM.tm_min) - CurrentMinuteInDay;
		else
			MinutesLeftInDay = 1440 - CurrentMinuteInDay;
		if (TimeoutMinutes == 0)
			Timeout << MinutesLeftInDay * 60 * 1000;
		else
			Timeout << TimeoutMinutes * 60 * 1000;
		OutputFormat.fill('0');
		OutputFormat << GetImageDirectory(DestinationDir, TheTime) << "/";
		OutputFormat.width(2);
		OutputFormat << UTC.tm_mon + 1;
		OutputFormat.width(2);
		OutputFormat << UTC.tm_mday;
		OutputFormat << "\%04d.jpg";
		FrameStart << GetLastImageNum(GetImageDirectory(DestinationDir, TheTime)) + 1;

		if (ConsoleVerbosity > 0)
		{
			std::cout << "[" << getTimeExcelLocal() << "]  OutputFormat: " << OutputFormat.str() << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]    FrameStart: " << FrameStart.str() << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]       Timeout: " << Timeout.str() << std::endl;
		}

		std::vector<std::string> mycommand;
		mycommand.push_back("raspistill");
		mycommand.push_back("--nopreview");
		mycommand.push_back("--thumb"); mycommand.push_back("none");
		mycommand.push_back("--width"); mycommand.push_back("1920");
		mycommand.push_back("--height"); mycommand.push_back("1080");
		mycommand.push_back("--timeout"); mycommand.push_back(Timeout.str());
		mycommand.push_back("--timelapse"); mycommand.push_back("60000");
		mycommand.push_back("--output"); mycommand.push_back(OutputFormat.str());
		mycommand.push_back("--framestart"); mycommand.push_back(FrameStart.str());
		if (ConsoleVerbosity > 0)
		{
			std::cout << "[" << getTimeExcelLocal() << "]        execvp:";
			for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
				std::cout << " " << *iter;
			std::cout << std::endl;
		}
		else
		{
			for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
				std::cerr << " " << *iter;
			std::cerr << std::endl;
		}
		std::vector<char*> args;
		for (auto arg = mycommand.begin(); arg != mycommand.end(); arg++)
			args.push_back((char*)arg->c_str());
		args.push_back(NULL);

		/* Attempt to fork */
		pid_t pid = fork();
		if (pid == 0)
		{
			/* A zero PID indicates that this is the child process */
			/* Replace the child fork with a new process */
			if (execvp(args[0], &args[0]) == -1)
				exit(EXIT_FAILURE);
		}
		else if (pid > 0)
		{
			/* A positive (non-negative) PID indicates the parent process */
			int CameraProgram_exit_status = 0;
			wait(&CameraProgram_exit_status);				/* Wait for child process to end */
			// https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/raspicam/RaspiStill.c
			// raspistill should exit with a 0 (EX_OK) on success, or 70 (EX_SOFTWARE)
			if ((EXIT_FAILURE == WEXITSTATUS(CameraProgram_exit_status)) || 
				(EX_SOFTWARE == WEXITSTATUS(CameraProgram_exit_status)) || 
				(255 == WEXITSTATUS(CameraProgram_exit_status))) // ERROR: the system should be configured for the legacy camera stack
			{
				mycommand.front() = "libcamera-still";
				mycommand.push_back("--verbose"); mycommand.push_back("0");
				mycommand.push_back("--continue-autofocus");
				if (ConsoleVerbosity > 0)
				{
					std::cout << "[" << getTimeExcelLocal() << "]        execvp:";
					for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
						std::cout << " " << *iter;
					std::cout << std::endl;
				}
				else
				{
					for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
						std::cerr << " " << *iter;
					std::cerr << std::endl;
				}
				args.clear();
				for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
					args.push_back((char*)iter->c_str());
				args.push_back(NULL);

				pid = fork();
				if (pid == 0)
				{
					/* A zero PID indicates that this is the child process */
					/* Replace the child fork with a new process */
					//HACK: Redirecting stderr to /dev/null so that libcamera app doesn't fill up syslog
					//FILE* devnull = fopen("/dev/null","w");
					//if (devnull != NULL)
					//{
					//	dup2(fileno(devnull), STDERR_FILENO);
					//	fclose(devnull);
					//}
					if (execvp(args[0], &args[0]) == -1)
						exit(EXIT_FAILURE);
				}
				else if (pid > 0)
				{
					/* A positive (non-negative) PID indicates the parent process */
					wait(&CameraProgram_exit_status);				/* Wait for child process to end */
					// https://github.com/raspberrypi/libcamera-apps/blob/main/apps/libcamera_still.cpp
					// libcamera-still exits with a 0 on success, or -1 if it catches an exception.
					if ((EXIT_FAILURE == WEXITSTATUS(CameraProgram_exit_status)) || 
						(255 == WEXITSTATUS(CameraProgram_exit_status))) // ERROR: *** unrecognised option '--continue-autofocus' ***
					{
						// One last try because the standard libcamera-still program doesn't have the --continue-autofocus option
						mycommand.pop_back();
						if (ConsoleVerbosity > 0)
						{
							std::cout << "[" << getTimeExcelLocal() << "]        execvp:";
							for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
								std::cout << " " << *iter;
							std::cout << std::endl;
						}
						else
						{
							for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
								std::cerr << " " << *iter;
							std::cerr << std::endl;
						}
						args.clear();
						for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
							args.push_back((char*)iter->c_str());
						args.push_back(NULL);

						pid = fork();
						if (pid == 0)
						{
							/* A zero PID indicates that this is the child process */
							/* Replace the child fork with a new process */
							//HACK: Redirecting stderr to /dev/null so that libcamera app doesn't fill up syslog
							//FILE* devnull = fopen("/dev/null", "w");
							//if (devnull != NULL)
							//{
							//	dup2(fileno(devnull), STDERR_FILENO);
							//	fclose(devnull);
							//}
							if (execvp(args[0], &args[0]) == -1)
								exit(EXIT_FAILURE);
						}
						else if (pid > 0)
						{
							/* A positive (non-negative) PID indicates the parent process */
							wait(&CameraProgram_exit_status);				/* Wait for child process to end */
							if (EXIT_SUCCESS == WEXITSTATUS(CameraProgram_exit_status) && (EXIT_SUCCESS == WTERMSIG(CameraProgram_exit_status)))
								rval = true;
						}
					}
					else if (EXIT_SUCCESS == WEXITSTATUS(CameraProgram_exit_status) && (EXIT_SUCCESS == WTERMSIG(CameraProgram_exit_status)))
						rval = true;
				}
			}
			else if (EXIT_SUCCESS == WEXITSTATUS(CameraProgram_exit_status) && (EXIT_SUCCESS == WTERMSIG(CameraProgram_exit_status)))
				rval = true;

			if (!rval)
			{
				std::cerr << mycommand.front() << " ended with exit (" << WEXITSTATUS(CameraProgram_exit_status) << ")" << std::endl;
				std::cerr << mycommand.front() << " ended with signal (" << WTERMSIG(CameraProgram_exit_status) << ")" << std::endl;
			}
			else if (ConsoleVerbosity > 0)
			{
				std::cout << "[" << getTimeExcelLocal() << "] " << mycommand.front() << " ended with exit (" << WEXITSTATUS(CameraProgram_exit_status) << ")" << std::endl;
				std::cout << "[" << getTimeExcelLocal() << "] " << mycommand.front() << " ended with signal (" << WTERMSIG(CameraProgram_exit_status) << ")" << std::endl;
			}
		}
		else
		{
			std::cerr << " Fork error! CameraProgram." << std::endl;  /* something went wrong */
		}
	}
	return(rval);
}
bool CreateDailyMovie(const std::string DailyDirectory, std::string VideoTextOverlay)
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
		if (!JPGfiles.empty())
		{
			sort(JPGfiles.begin(), JPGfiles.end());
			// What follows is a simple test that if there are newer images 
			// than the video files, empty the deque of video files and create 
			// a video file, possibly overwriting an older video.
			if (!MP4files.empty())
			{
				sort(MP4files.begin(), MP4files.end());
				struct stat LastJPGStat;
				if (0 == stat(JPGfiles.back().c_str(), &LastJPGStat))
				{
					struct stat LastMP4Stat;
					if (0 == stat(MP4files.back().c_str(), &LastMP4Stat))
					{
						if (LastJPGStat.st_mtim.tv_sec > LastMP4Stat.st_mtim.tv_sec)
							MP4files.clear();
					}
				}
			}
			if (MP4files.empty())
			{
				struct stat FirstJPGStat;
				if (0 == stat(JPGfiles.front().c_str(), &FirstJPGStat))
				{
					struct tm UTC;
					if (0 != localtime_r(&FirstJPGStat.st_mtim.tv_sec, &UTC))
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
							std::cout << "[" << getTimeExcelLocal() << "]   StillFormat: " << StillFormat.str() << std::endl;
							std::cout << "[" << getTimeExcelLocal() << "]    File Count: " << JPGfiles.size() << std::endl;
							std::cout << "[" << getTimeExcelLocal() << "] VideoFileName: " << VideoFileName.str() << std::endl;
						}
						std::vector<std::string> mycommand;
						mycommand.push_back("ffmpeg");
						mycommand.push_back("-hide_banner");
						mycommand.push_back("-loglevel"); mycommand.push_back("warning");
						mycommand.push_back("-r"); mycommand.push_back("30");
						mycommand.push_back("-i"); mycommand.push_back(StillFormat.str());
						auto found = VideoTextOverlay.find_first_of(":'\"\\");
						while (found != std::string::npos)
						{
							VideoTextOverlay.erase(found, 1);
							found = VideoTextOverlay.find_first_of(":'\"\\");
						}
						std::ostringstream vfParam;
						vfParam << "drawtext=font=mono:fontcolor=white:fontsize=40:y=main_h-text_h-10:x=10:text=%{metadata\\\\:DateTimeOriginal},drawtext=font=sans:fontcolor=white:fontsize=40:y=main_h-text_h-10:x=main_w-text_w-10:text=" << VideoTextOverlay;
						mycommand.push_back("-vf"); mycommand.push_back(vfParam.str());
						mycommand.push_back("-c:v"); mycommand.push_back("libx264");
						mycommand.push_back("-crf"); mycommand.push_back("23");
						mycommand.push_back("-preset"); mycommand.push_back("veryfast");
						mycommand.push_back("-movflags"); mycommand.push_back("+faststart");
						mycommand.push_back("-bf"); mycommand.push_back("2");
						mycommand.push_back("-g"); mycommand.push_back("15");
						mycommand.push_back("-pix_fmt"); mycommand.push_back("yuv420p");
						mycommand.push_back("-y");
						mycommand.push_back(VideoFileName.str());
						if (ConsoleVerbosity > 0)
						{
							std::cout << "[" << getTimeExcelLocal() << "]        execvp:";
							for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
								std::cout << " " << *iter;
							std::cout << std::endl;
						}
						else
						{
							std::cerr << " JPG File Count: " << JPGfiles.size() << std::endl;
							for (auto iter = mycommand.begin(); iter != mycommand.end(); iter++)
								std::cerr << " " << *iter;
							std::cerr << std::endl;
						}
						std::vector<char*> args;
						for (auto arg = mycommand.begin(); arg != mycommand.end(); arg++)
							args.push_back((char*)arg->c_str());
						args.push_back(NULL);

						pid_t pid_FFMPEG = fork();
						if (pid_FFMPEG == 0)
						{
							/* A zero PID indicates that this is the child process */
							/* Replace the child fork with a new process */
							if (execvp(args[0], &args[0]) == -1)
								exit(EXIT_FAILURE);
						}
						else if (pid_FFMPEG > 0)
						{
							/* A positive (non-negative) PID indicates the parent process */
							int ffmpeg_exit_status = 0;
							wait(&ffmpeg_exit_status);				/* Wait for child process to end */

							if (EXIT_SUCCESS != WEXITSTATUS(ffmpeg_exit_status))
							{
								std::cerr << mycommand.front() << " ended with exit (" << WEXITSTATUS(ffmpeg_exit_status) << ")" << std::endl;
								std::cerr << mycommand.front() << " ended with signal (" << WTERMSIG(ffmpeg_exit_status) << ")" << std::endl;
							}
							else if (ConsoleVerbosity > 0)
							{
								std::cout << "[" << getTimeExcelLocal() << "] " << mycommand.front() << " ended with exit (" << WEXITSTATUS(ffmpeg_exit_status) << ")" << std::endl;
								std::cout << "[" << getTimeExcelLocal() << "] " << mycommand.front() << " ended with signal (" << WTERMSIG(ffmpeg_exit_status) << ")" << std::endl;
							}

							if (EXIT_SUCCESS == WEXITSTATUS(ffmpeg_exit_status))
							{
								rval = true;
								// change file date on mp4 file to match the last jpg file
								struct stat LastJPGStatBuffer;
								if (0 == stat(JPGfiles.back().c_str(), &LastJPGStatBuffer))
								{
									struct utimbuf MP4TimeToSet;
									MP4TimeToSet.actime = LastJPGStatBuffer.st_mtim.tv_sec;
									MP4TimeToSet.modtime = LastJPGStatBuffer.st_mtim.tv_sec;
									utime(VideoFileName.str().c_str(), &MP4TimeToSet);
								}
							}
						}
						else
						{
							std::cerr << "Fork error! ffmpeg." << std::endl;  /* something went wrong */
						}
					}
				}
			}
		}
	}
	return(rval);
}
void CreateAllDailyMovies(const std::string DestinationDir, const std::string & VideoTextOverlay)
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
					// this time stuff is a hack so that if I'm restarting during the day I don't create the video for today.
					time_t TheTime;
					time(&TheTime);
					struct tm UTC;
					if (0 != localtime_r(&TheTime, &UTC))
					{
						std::ostringstream TodaysDirectoryName;
						TodaysDirectoryName.fill('0');
						TodaysDirectoryName << UTC.tm_year + 1900;
						TodaysDirectoryName.width(2);
						TodaysDirectoryName << UTC.tm_mon + 1;
						TodaysDirectoryName.width(2);
						TodaysDirectoryName << UTC.tm_mday;
						if (DirectoryName.compare(TodaysDirectoryName.str()) != 0)
						{
							std::string FullPath = DestinationDir + "/" + DirectoryName;
							Subdirectories.push_back(FullPath);
						}
					}
				}
			}
		closedir(dp);
		while (!Subdirectories.empty())
		{
			CreateDailyMovie(Subdirectories.front(), VideoTextOverlay);
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
std::string DestinationDir;
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
	std::cout << "    -G | --gps prefer gpsd lat/lon, if available, to command line" << std::endl;
	std::cout << "    -n | --name Text to display on the bottom right of the video" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hv:d:f:t:l:L:Gn:";
static const struct option long_options[] = {
	{ "help",no_argument,			NULL, 'h' },
	{ "verbose",required_argument,	NULL, 'v' },
	{ "destination",required_argument, NULL, 'd' },
	{ "freespace",required_argument,NULL, 'f' },
	{ "time",required_argument,		NULL, 't' },
	{ "lat",required_argument,		NULL, 'l' },
	{ "lon",required_argument,		NULL, 'L' },
	{ "gps",no_argument,			NULL, 'G' },
	{ "name",required_argument,		NULL, 'n' },
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
	std::string VideoOverlayText(ProgramVersionString);
	///////////////////////////////////////////////////////////////////////////////////////////////
	tzset();
	///////////////////////////////////////////////////////////////////////////////////////////////
	for (;;)
	{
		std::string TempString;
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
			TempString = std::string(optarg);
			if (ValidateDirectory(TempString))
				DestinationDir = TempString;
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
			try { Latitude = std::stod(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'L':
			try { Longitude = std::stod(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'G':
			UseGPSD = true;
			break;
		case 'n':
			VideoOverlayText = std::string(optarg);
			break;
		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	// I don't print the banner earlier because I haven't interpreted ConsoleVerbosity until I've parsed the parameters!
	if (ConsoleVerbosity > 0)
	{
		std::cout << "[" << getTimeExcelLocal() << "] " << ProgramVersionString << std::endl;
		std::ostringstream startupargs;
		for (auto index = 0; index < argc; index++)
			startupargs << " " << argv[index];
		std::cout << "[" << getTimeExcelLocal() << "] " << startupargs.str() << std::endl;
	}
	else
	{
		std::ostringstream startupargs;
		startupargs << ProgramVersionString << " (starting)" << std::endl;
		for (auto index = 0; index < argc; index++)
			startupargs << " " << argv[index];
		std::cerr << startupargs.str() << std::endl;
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (DestinationDir.empty())
	{
		usage(argc, argv);
		exit(EXIT_FAILURE);
	}
	CreateAllDailyMovies(DestinationDir, VideoOverlayText);
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Set up CTR-C signal handler
	typedef void (*SignalHandlerPointer)(int);
	SignalHandlerPointer previousHandler = signal(SIGINT, SignalHandlerSIGINT);
	bRun = true;
	while (bRun)
	{
		time_t LoopStartTime, SunriseNOAA, SunsetNOAA;
		time(&LoopStartTime);
		///////////////////////////////////////////////////////////////////////////////////////////////
		// If Latitude or Longitude not specified on command line try to get it from GPSD
		if ((Latitude == 0) || (Longitude == 0) || (UseGPSD))
			UseGPSD = getLatLon(Latitude, Longitude);
		///////////////////////////////////////////////////////////////////////////////////////////////
		if (getSunriseSunset(SunriseNOAA, SunsetNOAA, LoopStartTime, Latitude, Longitude))
		{
			SunriseNOAA -= 60 * 30; // Start half an hour before calculated Sunrise
			SunsetNOAA += 60 * 30;	// End half an hour after calculated Sunset
		}
		if (ConsoleVerbosity > 1)
		{
			double JulianDay = JulianDate2JulianDay(Time2JulianDate(LoopStartTime));
			double MeanSolarTime = getMeanSolarTime(JulianDay, Longitude);
			double SolarMeanAnomaly = getSolarMeanAnomaly(MeanSolarTime);
			double EquationOfTheCenter = getEquationOfTheCenter(SolarMeanAnomaly);
			double EclipticLongitude = getEclipticLongitude(SolarMeanAnomaly, EquationOfTheCenter);
			double SolarTransit = getSolarTransit(MeanSolarTime, SolarMeanAnomaly, EclipticLongitude);
			double DeclinationOfTheSun = getDeclinationOfTheSun(EclipticLongitude);
			double HourAngle = getHourAngle(Latitude, DeclinationOfTheSun);
			double Sunrise = getSunrise(SolarTransit, HourAngle);
			double Sunset = getSunset(SolarTransit, HourAngle);
			std::cout.precision(std::numeric_limits<double>::max_digits10);
			std::cout << "[" << getTimeExcelLocal() << "]         Julian Date: " << Time2JulianDate(LoopStartTime) << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]           Unix Time: " << LoopStartTime << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]         Julian Date: " << timeToExcelLocal(JulianDate2Time(Time2JulianDate(LoopStartTime))) << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]            Latitude: " << Latitude << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]           Longitude: " << Longitude << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]          Julian Day: " << JulianDay << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]       MeanSolarTime: " << MeanSolarTime << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]    SolarMeanAnomaly: " << SolarMeanAnomaly << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "] EquationOfTheCenter: " << EquationOfTheCenter << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]   EclipticLongitude: " << EclipticLongitude << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]        SolarTransit: " << timeToExcelLocal(JulianDate2Time(SolarTransit)) << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "] DeclinationOfTheSun: " << DeclinationOfTheSun << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]           HourAngle: " << HourAngle << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]             Sunrise: " << timeToExcelLocal(JulianDate2Time(Sunrise)) << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]              Sunset: " << timeToExcelLocal(JulianDate2Time(Sunset)) << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]        NOAA Sunrise: " << timeToExcelLocal(SunriseNOAA) << std::endl;
			std::cout << "[" << getTimeExcelLocal() << "]         NOAA Sunset: " << timeToExcelLocal(SunsetNOAA) << std::endl;
		}

		if (LoopStartTime < SunriseNOAA)
		{
			// if before sunrise we wait for sunrise, then loop back to the top
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeExcelLocal() << "] before Sunrise: " << timeToExcelLocal(SunriseNOAA) << " sleeping for " << (SunriseNOAA - LoopStartTime) / 60 << " minutes" << std::endl;
			else 
				std::cerr << "before Sunrise: " << timeToExcelLocal(SunriseNOAA) << " sleeping for " << (SunriseNOAA - LoopStartTime) / 60 << " minutes" << std::endl;
			sleep(SunriseNOAA - LoopStartTime);
		}
		else if (LoopStartTime > SunsetNOAA)
		{
			// If after Sunset we want to sleep till tomorrow
			struct tm UTC;
			if (0 != localtime_r(&LoopStartTime, &UTC))
			{
				int CurrentMinuteInDay = UTC.tm_hour * 60 + UTC.tm_min;
				int MinutesLeftInDay = 24*60 - CurrentMinuteInDay;
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeExcelLocal() << "] after Sunset: " << timeToExcelLocal(SunsetNOAA) << " sleeping for " << MinutesLeftInDay << " minutes" << std::endl;
				else
					std::cerr << "after Sunset: " << timeToExcelLocal(SunsetNOAA) << " sleeping for " << MinutesLeftInDay << " minutes" << std::endl;
				sleep(MinutesLeftInDay * 60);
			}
		}
		else
		{
			// largest file in sample was 1,310,523, multiply by minutes in day 1440, 1887153120, round up to 2000000000 or 2GB.
			GenerateFreeSpace(GigabytesFreeSpace, DestinationDir);
			bRun = CreateDailyStills(DestinationDir, LoopStartTime, SunsetNOAA);
			if (bRun)
				bRun = CreateDailyMovie(GetImageDirectory(DestinationDir, LoopStartTime), VideoOverlayText);
		}
	}
	// remove our special Ctrl-C signal handler and restore previous one
	signal(SIGINT, previousHandler);

	///////////////////////////////////////////////////////////////////////////////////////////////
	std::cerr << ProgramVersionString << " (exiting)" << std::endl;
	return(EXIT_SUCCESS);
}
