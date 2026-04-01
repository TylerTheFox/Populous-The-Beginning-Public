#pragma once
//***************************************************************************
// LbError.h : Error values
//***************************************************************************

//***************************************************************************
// <t>>TbError<< : A unique error value type
//***************************************************************************
typedef enum TbError // All library error values are negative
{
    // generic error codes
    LB_ERROR = -1, //	=-1	Generic error
    LB_OK = 0, //	=0	No error
	LB_OK_NO_DRAW = 2, // No Error, but do not draw.

    // specific error codes
    LBERR_OUT_OF_MEMORY = 100, //	memory errors

    LBERR_FILE_OPEN = 200, //	file errors
    LBERR_FILE_READ,
    LBERR_FILE_WRITE,
    LBERR_FILE_MAPPING,

    LBERR_INVALID_PARAMETER = 300, //	parameter errors
    LBERR_INVALID_HANDLE,
    LBERR_INVALID_DATA,
    LBERR_INVALID_SAMPLE,
    LBERR_INVALID_BANK,

    LBERR_NOT_INITIALISED = 400, //	misc errors
    LBERR_NOT_ENABLED,
    LBERR_CACHE,
    LBERR_DRIVER,
    LBERR_NO_BANK,
    LBERR_NO_HANDLE,

    LBERR_SOUND001 = 1000, //	mappable sound errors
    LBERR_SOUND002,
    LBERR_SOUND003,
    LBERR_SOUND004,
    LBERR_SOUND005,
    LBERR_SOUND006,
    LBERR_SOUND007,
    LBERR_SOUND008,
    LBERR_SOUND009,
    LBERR_SOUND010,
} TbError;

//***************************************************************************
//				END OF FILE
//***************************************************************************
