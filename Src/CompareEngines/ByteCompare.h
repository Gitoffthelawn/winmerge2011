/**
 * @file  ByteCompare.h
 *
 * @brief Declaration file for ByteCompare
 */
// ID line follows -- this is updated by SVN
// $Id$

#ifndef _BYTE_COMPARE_H_
#define _BYTE_COMPARE_H_

#include "FileTextStats.h"

struct FileLocation;
struct file_data;

namespace CompareEngines
{

/**
 * @brief A quick compare -compare method implementation class.
 * This compare method compares files in small blocks. Code assumes block size
 * is in range of 32-bit int-type.
 */
class ByteCompare : public DIFFOPTIONS
{
public:
	ByteCompare(const CDiffContext *);
	void SetFileData(int items, file_data *data);
	unsigned CompareFiles(FileLocation *location);

	FileTextStats m_textStats[2];

private:
	template<class CodePoint, int CodeShift>
	unsigned CompareFiles(stl_size_t x = 1, stl_size_t j = 0);
	const CDiffContext *const m_pCtxt;
	HANDLE m_osfhandle[2];
};

} // namespace CompareEngines

#endif // _BYTE_COMPARE_H_
