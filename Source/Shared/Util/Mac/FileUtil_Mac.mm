#include  "../FileUtil.h"

#import <Foundation/Foundation.h>
#import <Appkit/AppKit.h>

#include <string.h>
#include <sys/stat.h>
#include <string>
#include <vector>

bool CreateDir(const char* osDir)
{
	struct stat st = { 0 };

    int status = stat(osDir, &st);
    if (status == 0)
        return true; // already exists
    
	if (status == -1)
	{
		mkdir(osDir, 0700);
		return true;
	}

	return false;
}

char GetDirSep()
{
	return '/';
}

#define PLATFORM_MAX_PATH 256
static char g_appSupportPath[PLATFORM_MAX_PATH];
static char g_documentPath[PLATFORM_MAX_PATH];
static char g_bundlePath[PLATFORM_MAX_PATH];

const char *GetBundlePath(const char *fileName)
{
	static char path[PLATFORM_MAX_PATH];

	// App resources are presently being installed alongside the executable in
	// Contents/MacOS which is unusual. They're normally kept in the Resources
	// directory.
	snprintf(path, sizeof(path), "%s/Contents/MacOS/%s", g_bundlePath, fileName);
	return path;
}

const char *GetDocumentsPath(const char *fileName)
{
	static char path[PLATFORM_MAX_PATH];

	snprintf(path, sizeof(path), "%s/%s", g_documentPath, fileName);
	return path;
}

const char *GetAppSupportPath(const char *fileName)
{
	static char path[PLATFORM_MAX_PATH];

	snprintf(path, sizeof(path), "%s/%s", g_appSupportPath, fileName);
	return path;
}

void FileInit(void)
{
	// Find bundle path
	NSString *bundlePath = [[NSBundle mainBundle] bundlePath];

	NSFileManager *defaultFileManager = [NSFileManager defaultManager];

	// Find document path
	NSArray *documentsPaths = [defaultFileManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask];
	NSString *documentsPath = [documentsPaths firstObject];

	// Find application support path
	NSArray *applicationSupportDirectory = [defaultFileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
	NSString *spectrumAnalyserDirectory = nil;
	if ([applicationSupportDirectory count] > 0)
	{
		NSString *bundleID = [[NSBundle mainBundle] bundleIdentifier];
		NSURL *spectrumAnalyserDirectoryURL = nil;
		NSError *theError = nil;

		spectrumAnalyserDirectoryURL = [[applicationSupportDirectory objectAtIndex:0] URLByAppendingPathComponent:bundleID];

		if (![defaultFileManager createDirectoryAtURL:spectrumAnalyserDirectoryURL
						  withIntermediateDirectories:YES
										   attributes:nil
												error:&theError])
		{
			NSLog(@"Create directory error: %@", theError);
			return;
		}

		spectrumAnalyserDirectory = [spectrumAnalyserDirectoryURL path];
	}

	snprintf(g_bundlePath, PLATFORM_MAX_PATH, "%s", [bundlePath fileSystemRepresentation]);
	snprintf(g_documentPath, PLATFORM_MAX_PATH, "%s", [documentsPath fileSystemRepresentation]);
	snprintf(g_appSupportPath, PLATFORM_MAX_PATH, "%s", [spectrumAnalyserDirectory fileSystemRepresentation]);
}

bool OpenFileDialog(std::string &outFile, const char *pInitialDir, const char *pFilter)
{
	@autoreleasepool {
		NSOpenPanel *panel = [NSOpenPanel openPanel];
		[panel setCanChooseFiles:YES];
		[panel setCanChooseDirectories:NO];
		[panel setAllowsMultipleSelection:NO];

		if (pInitialDir && pInitialDir[0] != '\0') {
			NSString *dir = [NSString stringWithUTF8String:pInitialDir];
			[panel setDirectoryURL:[NSURL fileURLWithPath:dir isDirectory:YES]];
		}

		// Parse Win32-style filter: pairs of "Description\0*.ext1;*.ext2\0" terminated by double \0
		if (pFilter) {
			NSMutableArray<NSString *> *extensions = [[NSMutableArray alloc] init];
			const char *p = pFilter;
			bool hasWildcard = false;
			while (*p) {
				// Skip description string
				p += strlen(p) + 1;
				if (!*p) break;
				// Parse wildcard string (e.g. "*.mzf;*.wav;*.mzq")
				std::string wildcards(p);
				p += strlen(p) + 1;

				size_t pos = 0;
				while (pos < wildcards.size()) {
					size_t semi = wildcards.find(';', pos);
					if (semi == std::string::npos) semi = wildcards.size();
					std::string wc = wildcards.substr(pos, semi - pos);
					pos = semi + 1;

					// Extract extension from "*.ext"
					size_t dot = wc.rfind('.');
					if (dot != std::string::npos && dot + 1 < wc.size()) {
						std::string ext = wc.substr(dot + 1);
						if (ext == "*") {
							hasWildcard = true;
						} else {
							[extensions addObject:[NSString stringWithUTF8String:ext.c_str()]];
						}
					}
				}
			}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
			if (!hasWildcard && [extensions count] > 0) {
				[panel setAllowedFileTypes:extensions];
			}
#pragma clang diagnostic pop
		}

		NSModalResponse result = [panel runModal];
		if (result == NSModalResponseOK) {
			NSURL *url = [[panel URLs] firstObject];
			outFile = [[url path] UTF8String];
			return true;
		}
	}
	return false;
}
