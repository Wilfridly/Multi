///////////////////////////////////////////////////////////////////////////
// File : pibus_segment_table.h
// Date : 08/01/2010
// author : Alain Greiner 
// Copyright : UPMC - LIP6
// This program is released under the GNU public license
///////////////////////////////////////////////////////////////////////////
// This object is used to describe the memory segmentation and address 
// decoding scheme for shared memory multiprocessor architectures using
// the PIBUS as system interconnect. 
// 
// The PIBUS ADDRESS is structured as follows : | MSB | UNUSED | OFFSET |
// The MSB bits of the  ADDRESS are decoded by the PIBUS_BCU  component
// to select the proper PIBUS target. The same MSB bits are decoded by the
// PIBUS_XCACHE component to decide if the address is cachable or not.
//
// The MSB bits of the address define 2**MSB pages. Each page contains
// at most one segment and the segment size cannot be larger than
// 2**(32-MSB) bytes.  
// Several pages (i.e. several segments) can be mapped to the same
// PIBUS target (example : PIBUS_SIMPLE_RAM).
// The number of PIBUS targets cannot be larger than 32.
// The number of MSB bits cannot be larger than 8 (256 segments).
// 
// Each segment descriptor contains the following fields:
// - const char		*name	: segment name
// - size_t 	base	: base address
// - size_t 	size	: segment size (in bytes)
// - size_t	index	: target index
// - bool			cached  : cached when true
//
// The information defined in the PibusSegmentTable is used by the
// constructor of the following hardware components:
// - The constructors of the PIBU_BCU use it to build the Target ROM 
//   implementing the target selection (decoding the MSB address bits).
// - The constructors of all PIBUS targets use it to implement the 
//   segmentation violation detection mechanism.
// - The constructor of the PIBUS_MULTIRAM use it to allocate the 
//   buffers representing the memory for the segments.
// - The constructor of the PIBUS_XCACHE use it to build the Cached ROM
//   implementing the CACHED flag (decoding the MSB address bits).
///////////////////////////////////////////////////////////////////////////

#ifndef PIBUS_SEGMENT_TABLE_H
#define PIBUS_SEGMENT_TABLE_H

#include <list>

namespace soclib { namespace common {

////////////////////////
class SegmentTableEntry
{

private:

const char*	name;
size_t 	base;
size_t 	size;
size_t	target;
bool 		cached;

public:

/////////////////////////////////////////
SegmentTableEntry(	const char*	nm,
			size_t		ba, 
			size_t		sz,
			size_t		tg,
			bool		ca)
{
	name	= nm;
	base	= ba;
	size	= sz;
	target	= tg;
	cached	= ca;
}; // end constructor

////////////
void print()
{ 
	printf("segment : %s , base = 0x%x , size = 0x%x , target = %d , cached = %d\n",
		name, base, size,  (int)target, (int)cached);
};
/////////////////////
const char *getName()
{
	return name;
};
////////////////
size_t getBase()
{
	return base;
};
////////////////
size_t getSize()
{
	return size;
};
///////////////////////
size_t getTargetIndex()
{
	return target;
};
////////////////
bool getCached()
{
	return cached;
};

};	// end class SegmentTable Entry 


//////////////////////////////////////////////////////////////////////////////////
//			PibusSegmentTable definition
//////////////////////////////////////////////////////////////////////////////////

class PibusSegmentTable {
	
private :

std::list<SegmentTableEntry> 	m_segment_list;
size_t*				m_target_table;
bool				m_target_table_called;
bool*				m_cached_table;
bool				m_cached_table_called;
int				m_MSB_number;
bool				m_MSB_number_called;

public:
	
//////////////////////////////
void setMSBnumber (int number)  
{
	if ((number < 0)||(number > 8))
		{
		std::cerr << "ERROR in the Segment Table :" << std::endl ;
		std::cerr << "MSB number must be in the [1...8] range !" << std::endl ;
		exit(0);
		}
		m_MSB_number_called=true;
		m_MSB_number=number;
}

//////////////////
int getMSBnumber()
{
	return m_MSB_number;
}

////////////////////////////////////////////
std::list<SegmentTableEntry> getSegmentList()
{
	return(m_segment_list);
}
	
///////////////////////////////////
void addSegment(const char*	nm,
		size_t 		ba,
		size_t 		sz,
		size_t 		tg,
		bool	 	ca) 
{
				
	if(m_MSB_number_called == false) 
	{
		std::cerr << "ERROR in the Segment Table :" << std::endl ;
		std::cerr << "The MSB number must be defined" << std::endl ;
		std::cerr << "before any segment declaration !" << std::endl ;
		exit(0);
	}
	if(sz > (size_t)(1 << (32 - m_MSB_number))) 
	{
		std::cerr << "ERROR in the Segment Table :" << std::endl ;
		std::cerr << "The size of the segment " << nm << std::endl ;
		std::cerr << "is larger than the page size : 2**(32-MSB) bytes !" << std::endl ;
		exit(0);
	}
	if(tg > 31) 
	{
		std::cerr << "ERROR in the Segment Table :" << std::endl ;
		std::cerr << "The number of PIBUS targets cannot be larger than 32" << std::endl;
		std::cerr << "The target index for the segment " << nm << std::endl ;
		std::cerr << "is larger than 31 !" << std::endl ;
		exit(0);
	}
	std::list<SegmentTableEntry>::iterator seg;
	for (seg = m_segment_list.begin() ; seg != m_segment_list.end() ; ++seg) 
	{
		const char	*name   = (*seg).getName();
		size_t	base    = (*seg).getBase();
		size_t	size    = (*seg).getSize();
		size_t	index   = (*seg).getTargetIndex();
		if((ba >> (32 - m_MSB_number)) == (base >> (32 - m_MSB_number))) 
		{
			// two segments in the same page
			if(tg != index) // with different target index
                        {
				std::cerr << "ERROR in the Segment Table:" << std::endl ;
				std::cerr << "Segment " << name << " and segment " << nm << std::endl;
				std::cerr << "cannot be in the same page, because " << std::endl;
				std::cerr << "they are allocated to different targets" << std::endl;
				exit(0);
			}
			if(((base+size) > ba) && ((ba+sz) > base)) // intersecting
                        {
				std::cerr << "ERROR in the Segment Table:" << std::endl ;
				std::cerr << "Segment " << name << " and segment " << nm << std::endl;
				std::cerr << "are intersecting ! " << std::endl;
				exit(0);
			}
		}
	} 

	SegmentTableEntry *segment = new SegmentTableEntry(nm,ba,sz,tg,ca);
	m_segment_list.push_back(*segment);
} // end addSegment()

////////////
void print()
{
	std::cout << "\n                 SEGMENT  TABLE\n\n" ;
	std::list<SegmentTableEntry>::iterator iter;
	for (iter = m_segment_list.begin() ; iter != m_segment_list.end() ; ++iter)
	{
		(*iter).print();
	}
	std::cout << std::endl ;
}

///////////////////////	
void printTargetTable()
{
	if(m_target_table_called == false) 
	{
		std::cerr << "ERROR in printTargetTable :" << std::endl ;
		std::cerr << "The Target table has not been created yet" << std::endl ;
		exit(0);
	}
	std::cout << "\n The Target Table is indexed by the MSB bits of the PIBUS address\n\n" ;
	int size = 1 << m_MSB_number;
	for(int index = 0 ; index < size ; index++) 
	{
		std::cout << "Target Table [" << index << "] = " << m_target_table[index] << std::endl;
	}
	std::cout << std::endl ;
}
	
//////////////////////
void printCachedTable()
{
	if(m_cached_table_called == false) 
	{
		std::cerr << "ERROR in printCachedTable :" << std::endl ;
		std::cerr << "The Cached table has not been created yet" << std::endl ;
		exit(0);
	}
	std::cout << "\n The Cached Table is indexed by the MSB bits of the PIBUS address\n\n" ;
	int size = 1 << m_MSB_number;
	for(int index = 0 ; index < size ; index++) 
	{
		std::cout << "Cached Table [" << index << "] = " << m_cached_table[index] << std::endl;
	}
	std::cout << std::endl ;
}
	
/////////////////////////////////////////////////////////////////////////
// returns a pointer on the Target ROM (indexed by the address MSB bits)
size_t *getTargetTable() 
{
	if (m_MSB_number_called == false) 
	{
		std::cerr << "ERROR in the Segment Table:" << std::endl ;
		std::cerr << "the MSB number has not been defined !" << std::endl ;
		exit(0);
	}
	int size  = 1 << m_MSB_number;
	if(m_target_table_called == false) 	// not created yet
        {
		m_target_table = new size_t[size];
		m_target_table_called = true;
		
		//default target index is 0
		for (int page = 0 ; page < size ; page++) 
		{
			m_target_table[page] = 0;
		} // end for page
		
		// loop on the segment list
		std::list<SegmentTableEntry>::iterator seg;
		for (seg = m_segment_list.begin() ; seg != m_segment_list.end() ; ++seg) 
		{
			size_t	base    = (*seg).getBase();
			size_t	target	= (*seg).getTargetIndex();
			size_t	page	= base >> (32-m_MSB_number) ;
			m_target_table[page]	= target;
		} // end for seg
	} // end if
	return m_target_table;
}  // end getTargetTable()

/////////////////////////////////////////////////////////////////////////
// returns a pointer on the Cached ROM (indexed by the address MSB bits) 
bool *getCachedTable() 
{
	if (m_MSB_number_called == false) 
	{
		std::cerr << "ERROR in the Segment Table:" << std::endl ;
		std::cerr << "the MSB number has not been defined !" << std::endl ;
		exit(0);
	}
	int size  = 1 << m_MSB_number;
	if(m_cached_table_called == false) 	// not created yet
	{
		m_cached_table = new bool[size];
		m_cached_table_called = true;
	
		// default value is uncached
		for (int page = 0 ; page < size ; page++) 
		{
			m_cached_table[page] = false;
		}
	
		// loop on the segment list
		std::list<SegmentTableEntry>::iterator seg;
		for (seg = m_segment_list.begin() ; seg != m_segment_list.end() ; ++seg) 
		{
			size_t	base    = (*seg).getBase();
			bool		cached	= (*seg).getCached();
			size_t	page	= base >> (32-m_MSB_number) ;
			m_cached_table[page] = cached;
		} // end for seg
	} // end if
	return m_cached_table;
}  // end getCachedTable()

////////////////////////////////////////////////////////////////////////
// returns the list of all segments allocated to a given PIBUS target.
std::list<SegmentTableEntry> getTargetSegmentList(size_t target) 
{
	std::list<SegmentTableEntry> list;
	std::list<SegmentTableEntry>::iterator seg;
	// loop on all segments
	for (seg = m_segment_list.begin() ; seg != m_segment_list.end() ; ++seg) 
        {
		size_t tg =(*seg).getTargetIndex();
		if (tg == target) list.push_back(*seg);
	} // en for seg
	return(list);
} // end getTargetSegmentList()

/////////////////////////////////////////////////////////////////////////////
// cheks if all target indexes registered in the segment table are consistent.
bool isAllBelow(size_t ntarget) 
{
	std::list<SegmentTableEntry>::iterator seg;
	// loop on all segments
	for (seg = m_segment_list.begin() ; seg != m_segment_list.end() ; ++seg) 
        {
		size_t tg =(*seg).getTargetIndex();
		if (tg >= ntarget) return false;
	} 
	return true;
} // end getTargetSegmentList()

////////////////////
PibusSegmentTable()
{
	m_MSB_number_called	= false;
	m_cached_table_called	= false;
	m_target_table_called	= false;
}  // end constructor 

}; // end PibusSegmentTable

}} // end namespaces

#endif

