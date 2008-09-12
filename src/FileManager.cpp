#include <assert.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <cstdlib> 

#include <rlog/rlog.h>

#include "FileManager.hpp"

FileManager::FileManager()
{
}

FileManager::~FileManager()
{
	bool				 flag = false;
	CFile				*file;
	set<File *, ltFile>::iterator	 it = m_files.begin();

	// FUSE should allow umount only when filesystem is not
	// in use. Therefore if m_files is not empty and some files
	// have m_refs bigger than zero, we were run in debug mode
	// and have no responsibility for any damage
	// when user hits CTRL+C.
	//
	while (it != m_files.end())
	{
		file = dynamic_cast<CFile*> (*it);
		
		if ((file->m_refs > 0) & !flag)
		{
			rError("FuseCompress killed while mounted \
			        with some opened files.");
			
			flag = true;
		}
		
		m_files.erase(it++);
		delete file;
	}
}

void FileManager::Put(CFile *file)
{
	m_mutex.Lock();

	file->m_refs--;

	// TODO: Invent some policy to decide when and why to
	// call delete file...
	//
	if (file->m_refs < 1)
	{
		m_files.erase(file);
		delete file;
	}

	m_mutex.Unlock();
}

CFile *FileManager::Get(const char *name, bool create)
{
	struct stat			 st;
	CFile				*file = NULL;
	set<File *, ltFile>::iterator	 it;

	// Get inode number from the name. If 'name' is symbolic link,
	// retrieve inode number of the file it points to.
	// 
	if (stat(name, &st) == -1)
	{
		if ((errno != ENOENT) && (errno != ELOOP))
		{
			// File doesn't exist.
			// 
			return NULL;
		}
		
		// 'name' may be nonexistant file ot symbolic link that points
		// to nonexistant file. Figure out what 'name' is really.
		//
		if ((lstat(name, &st) == -1) || (! S_ISLNK(st.st_mode)))
		{
			// File doesn't exist.
			// 
			return NULL;
		}
		
		// Preserve inode number - it's our index and node number
		// of link is better than nothing.
		//
		// Set the size to zero to let the Compress decide whatever
		// it use compressed or uncompressed FileRaw.
		//
		st.st_size = 0;
	}

	File search(&st);
	
	m_mutex.Lock();
	
	it = m_files.find(&search);
	
	if (it != m_files.end())
	{
		file = dynamic_cast<CFile*> (*it);
		
		if (create)
		{
			file->m_refs++;
		}
	}
	else
		if (create)
		{
			file = new (std::nothrow) CFile(&st);
			if (!file)
			{
				rError("No memory to allocate object of "
				       "FileRaw interface");
				abort();
			}

			m_files.insert(file);
		}

	m_mutex.Unlock();

	return file;
}
