#ifndef __BITOPS_H
#define __BITOPS_H

#define BYTE_SHIFT	3
#define WORD_SHIFT	4
#define	LONG_SHIFT	5

extern __inline int test_bit( int nr, unsigned int *data )
{
	int n = nr >> LONG_SHIFT ;
	int ret ; 

	data += n ;
	nr -= n << LONG_SHIFT;	
    if( *data & (1UL << nr ) ) ret = 1 ;
    else ret = 0 ;

	return ret ;
}

extern __inline void set_bit( int nr, unsigned int *data )
{
	int n = nr >> LONG_SHIFT;

	data += n ;
	nr -= n << LONG_SHIFT ;
    *data |= (1UL << nr ) ;
}

extern __inline void clear_bit( int nr, unsigned int *data )
{
	int n = nr >> LONG_SHIFT;

	data += n ;
	nr -= n << LONG_SHIFT ;
    *data &= ~(1UL << nr) ;
}

extern __inline void change_bit( int nr, unsigned int *data )
{
	int n = nr >> LONG_SHIFT;

	data += n ;
	nr -= n << LONG_SHIFT ;

    if( *data & (1UL << nr ) ) clear_bit( nr, data ) ;
    else set_bit( nr, data ) ;
}

extern __inline int test_and_set_bit( int nr, unsigned int *data )
{
    int ret = 0 ;
	int n = nr >> LONG_SHIFT;

	data += n ;
	nr -= n << LONG_SHIFT ;

    if( *data & (1UL << nr ) ) ret = 1 ;
    set_bit( nr, data ) ;
	    
    return ret ;
}

extern __inline int test_and_clear_bit( int nr, unsigned int *data )
{
    int ret = 0 ;
	int n = nr >> LONG_SHIFT;

	data += n ;
	nr -= n << LONG_SHIFT ;
    if( *data & (1UL << nr ) ) ret = 1 ;
    clear_bit( nr, data ) ;
    
    return ret ;
}

extern __inline int test_and_change_bit( int nr, unsigned int *data )
{
    int ret = 0 ;
	int n = nr >> LONG_SHIFT;

	data += n ;
	nr -= n << LONG_SHIFT ;

    if( *data & (1UL << nr ) ) {
    	clear_bit( nr, data ) ;
		ret = 1 ;
    }
    else set_bit( nr, data ) ;
    
    return ret ;
}

#endif
