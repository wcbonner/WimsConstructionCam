#include <algorithm>
#include <arpa/inet.h>
#include <cfloat>
#include <climits>
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
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h> // For close()
#include <utime.h>
#include <vector>

/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("WimConstructionCam Version 1.20220625-1 Built on: " __DATE__ " at " __TIME__);
int ConsoleVerbosity = 1;
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
std::string GetImageDirectory(const std::string DestinationDir, const time_t& TheTime)
{
	std::ostringstream OutputDirectoryName;
	OutputDirectoryName << DestinationDir;
	struct tm UTC;
	if (0 != localtime_r(&TheTime, &UTC))
	{
		OutputDirectoryName.fill('0');
		OutputDirectoryName << UTC.tm_year + 1900;
		OutputDirectoryName.width(2);
		OutputDirectoryName << UTC.tm_mon + 1;
		OutputDirectoryName.width(2);
		OutputDirectoryName << UTC.tm_mday;
		OutputDirectoryName << "/";
	}

	struct stat buffer;
	if (0 != stat(OutputDirectoryName.str().c_str(), &buffer))
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
	struct statfs64 buffer2;
	if (0 == statfs64(DestinationDir.c_str(), &buffer2))
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
				LastImageNum = atoi(LastFile.substr(4, 4).c_str());
			}
		}
	}
	return(LastImageNum);
}
/////////////////////////////////////////////////////////////////////////////
void GenerateFreeSpace(const unsigned long long MinFreeSpace, const int OutputFolderNum = 100)
{
	std::ostringstream OutputDirectoryName;
	OutputDirectoryName << "/media/BONEBOOT/DCIM/";
	OutputDirectoryName.fill('0');
	OutputDirectoryName.width(3);
	OutputDirectoryName << OutputFolderNum;
	OutputDirectoryName << "WIMBO";
	struct statfs64 buffer2;
	if (0 == statfs64(OutputDirectoryName.str().c_str(), &buffer2))
	{
		std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " optimal transfer block size: " << buffer2.f_bsize << std::endl;
		std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " total data blocks in file system: " << buffer2.f_blocks << std::endl;
		std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " free blocks in fs: " << buffer2.f_bfree << std::endl;
		std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " free blocks avail to non-superuser: " << buffer2.f_bavail << std::endl;
		std::cout << "[" << getTimeISO8601() << "] " << OutputDirectoryName.str() << " Drive Size: " << buffer2.f_bsize * buffer2.f_blocks << " Free Space: " << buffer2.f_bsize * buffer2.f_bavail << std::endl;
		DIR* dp;
		if ((dp = opendir(OutputDirectoryName.str().c_str())) != NULL)
		{
			//std::map<time_t, string> files;
			std::deque<std::string> files;
			struct dirent* dirp;
			while ((dirp = readdir(dp)) != NULL)
				if (DT_REG == dirp->d_type)
				{
					std::string filename = OutputDirectoryName.str() + "/" + std::string(dirp->d_name);
					files.push_back(filename);
				}
			closedir(dp);
			if (!files.empty())
				sort(files.begin(), files.end());
			//for (std::map<time_t, string>::const_iterator filename = files.begin(); filename != files.end(); filename++)
			//	cout << "[" << timeToISO8601(LogFileTime) << "] " << filename->second << endl;
			while ((!files.empty()) && (buffer2.f_bsize * buffer2.f_bavail < MinFreeSpace))	// This loop will make sure that there's 32MB of free space on the drive.
			{
				struct stat buffer;
				if (0 == stat(files.begin()->c_str(), &buffer))
					if (0 == remove(files.begin()->c_str()))
						std::cout << "[" << getTimeISO8601() << "] File Deleted: " << *files.begin() << "(" << buffer.st_size << ")" << std::endl;
				//cout << "[" << timeToISO8601(LogFileTime) << "] " << dirp->d_name << " st_ctime: " << buffer.st_ctime << endl;
				//cout << "[" << timeToISO8601(LogFileTime) << "] " << dirp->d_name << " st_mtime: " << buffer.st_mtime << endl;
				//cout << "[" << timeToISO8601(LogFileTime) << "] " << dirp->d_name << " st_atime: " << buffer.st_atime << endl;
				//files[buffer.st_mtime] = dirp->d_name;
				files.pop_front();
				if (0 != statfs64(OutputDirectoryName.str().c_str(), &buffer2))
					break;
				//for (std::vector<string>::const_iterator filename = files.begin(); filename != files.end(); filename++)
				//	cout << "[" << timeToISO8601(LogFileTime) << "] " << *filename << endl;
			}
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
static void usage(int argc, char** argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  " << ProgramVersionString << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -h | --help          Print this message" << std::endl;
	std::cout << "    -v | --verbose level stdout verbosity level [" << ConsoleVerbosity << "]" << std::endl;
	std::cout << "    -d | --destination location pictures will be stored [" << ConsoleVerbosity << "]" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hv:d:";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "destination",	required_argument, NULL, 'd' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    printf("hello from %s!\n", "WimsConstructionCam");
	std::cout << "raspistill --nopreview --thumb none --width 1920 --height 1080 --timeout 57600000 --timelapse 60000 --output DCIM/img%05d.jpg" << std::endl;
	std::cout << "ffmpeg.exe -hide_banner -r 30 -i D:\DCIM\20220626\0628-%03d.JPG -vf drawtext=fontfile=C\\:/WINDOWS/Fonts/consola.ttf:fontcolor=white:fontsize=80:y=main_h-text_h-50:x=main_w-text_w-50:text=WimsWorld,drawtext=fontfile=C\\:/WINDOWS/Fonts/consola.ttf:fontcolor=white:fontsize=80:y=main_h-text_h-50:x=50:text=%{metadata\\:DateTimeOriginal} -c:v libx265 -crf 23 -preset veryfast -movflags +faststart -bf 2 -g 15 -pix_fmt yuv420p -n C:\Users\Wim\Videos\20220626-1080p30.mp4" << std::endl;
	std::string DestinationDir("/home/wim/DCIM/");
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
		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
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
	// Set up CTR-C signal handler
	typedef void (*SignalHandlerPointer)(int);
	SignalHandlerPointer previousHandler = signal(SIGINT, SignalHandlerSIGINT);
	bRun = true;
	for (int OutputFolderNum = 100; (OutputFolderNum < 1000) && bRun; OutputFolderNum++)
	{
		for (int OutputImageNum = 1 + GetLastImageNum(OutputFolderNum); (OutputImageNum < 10000) && bRun; OutputImageNum++)
		{
			GenerateFreeSpace(1300000000ll, OutputFolderNum);
			std::string OutputFilename(GenerateLogFileName(OutputImageNum, OutputFolderNum));
			if (0 != mkfifo("/tmp/Wim_C920_FFMPEG", S_IRWXU))
				std::cerr << "[" << getTimeISO8601() << "] FiFo NOT created Successfully" << std::endl;
			else
				std::cerr << "[" << getTimeISO8601() << "] FiFo created Successfully" << std::endl;
			/* Attempt to fork */
			pid_t pid = fork();
			if (pid == -1)
			{
				std::cerr << "Fork error! Exiting." << std::endl;  /* something went wrong */
				exit(1);
			}
			else if (pid == 0)
			{
				std::ostringstream ssMetaDataTitle;
				ssMetaDataTitle << "title=Wim's BoneCam " << getTimeISO8601();
				std::string ssAddress("rtp://192.168.0.10:8090/");
				if (Broadcast)
					ssAddress = std::string("rtp://239.8.8.8:8090/");
				/* A zero PID indicates that this is the child process */
				/* Replace the child fork with a new process */
				if (force_format == 4)
				{
					ssAddress = std::string("udp://192.168.0.10:8090/");
					if (execlp("ffmpeg", "ffmpeg", "-nostdin", "-i", "/tmp/Wim_C920_FFMPEG", "-vcodec", "copy", "-f", "mjpeg", ssAddress.c_str(), "-metadata", ssMetaDataTitle.str().c_str(), "-vcodec", "copy", "-f", "mjpeg", OutputFilename.c_str(), NULL) == -1)
					{
						std::cerr << "execlp Error! Exiting." << std::endl;
						exit(1);
					}
				}
				else
				{
					if (execlp("ffmpeg", "ffmpeg", "-nostdin", "-i", "/tmp/Wim_C920_FFMPEG", "-vcodec", "copy", "-f", "rtp", ssAddress.c_str(), "-metadata", ssMetaDataTitle.str().c_str(), "-vcodec", "copy", OutputFilename.c_str(), NULL) == -1)
					{
						std::cerr << "execlp Error! Exiting." << std::endl;
						exit(1);
					}
				}
			}
			else
			{
				/* A positive (non-negative) PID indicates the parent process */
				open_device();
				init_device();
				start_capturing();
				//mainloop();
				int pipe_fd = open("/tmp/Wim_C920_FFMPEG", O_WRONLY);
				if (pipe_fd <= 0)
					std::cerr << "[" << getTimeISO8601() << "] FiFo NOT opened Successfully" << std::endl;
				else
				{
					std::cerr << "[" << getTimeISO8601() << "] FiFo opened Successfully" << std::endl;
					time_t CurrentTime;
					time(&CurrentTime);
					time_t FileStartTime = CurrentTime;
					while ((difftime(CurrentTime, FileStartTime) < (60 * 60)) && (bRun))
					{
						fd_set fds;
						struct timeval tv;
						int r;

						FD_ZERO(&fds);
						FD_SET(fd, &fds);

						/* Timeout. */
						tv.tv_sec = 2;
						tv.tv_usec = 0;

						r = select(fd + 1, &fds, NULL, NULL, &tv);
						if (-1 == r)
						{
							if (EINTR == errno)
								continue;
							errno_exit("select");
						}
						if (0 == r)
						{
							std::cerr << "select timeout" << std::endl;
							exit(EXIT_FAILURE);
						}
						struct v4l2_buffer buf;
					WIMLABEL:
						CLEAR(buf);
						buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
						buf.memory = V4L2_MEMORY_MMAP;

						if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
						{
							switch (errno)
							{
							case EAGAIN:
								goto WIMLABEL;
							case EIO:
								/* Could ignore EIO, see spec. */
								/* fall through */
							default:
								errno_exit("VIDIOC_DQBUF");
							}
						}
						else
						{
							assert(buf.index < n_buffers);
							write(pipe_fd, buffers[buf.index].start, buf.bytesused);
						}
						if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
							errno_exit("VIDIOC_QBUF");
						time(&CurrentTime);
					}
					stop_capturing();
					uninit_device();
					close_device();
					close(pipe_fd);		/* Close side of pipe I'm writing to, to get the child to recognize it's gone away */
					std::cerr << "\n[" << getTimeISO8601() << "] Pipe Closed, Waiting for FFMPEG to exit" << std::endl;
				}
				int ffmpeg_exit_status;
				wait(&ffmpeg_exit_status);				/* Wait for child process to end */
				std::cerr << "[" << getTimeISO8601() << "] FFMPEG exited with a  " << ffmpeg_exit_status << " value" << std::endl;
				if (0 == remove("/tmp/Wim_C920_FFMPEG"))
					std::cerr << "[" << getTimeISO8601() << "] FiFo removed Successfully" << std::endl;
				else
					std::cerr << "[" << getTimeISO8601() << "] FiFo NOT removed Successfully" << std::endl;
			}
		}
	}
	// remove our special Ctrl-C signal handler and restore previous one
	signal(SIGINT, previousHandler);

	///////////////////////////////////////////////////////////////////////////////////////////////
	std::cerr << ProgramVersionString << " (exiting)" << std::endl;
	return(EXIT_SUCCESS);
}