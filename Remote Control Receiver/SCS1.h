#pragma once
class SCS1 {
public:
	//static const short DO NOT USE	=0x0
	//static const short DO NOT USE	=(short)0xE000
	/*static const short 	=0x29;   // ~  `
	static const short 	=0x2	;   // !  1
	static const short 	=0x3	;   // @  2
	static const short 	=0x4	;   // #  3
	static const short 	=0x5	;   // $  4
	static const short 	=0x6	;   // %  5
	static const short 	=0x7	;   // ^  6
	static const short 	=0x8	;   // &  7
	static const short 	=0x9	;   // *  8
	static const short 	=0x0A;   // (  9
	static const short 	=0x0B;   // )  0
	static const short 	=0x0C;   // _  -
	static const short 	=0x0D;   // +  =*/
	static const short Backspace = 0x0E;
	static const short Tab = 0x0F;
	static const short Q = 0x10;
	static const short W = 0x11;
	static const short E = 0x12;
	static const short R = 0x13;
	static const short T = 0x14;
	static const short Y = 0x15;
	static const short U = 0x16;
	static const short I = 0x17;
	static const short O = 0x18;
	static const short P = 0x19;
	/*static const short 	=0x1A;   // {  [
	static const short 	=0x1B;   // }  ]
	static const short 	=0x2B;   // |  \ */
	static const short CapsLock = 0x3A;
	static const short A = 0x1E;
	static const short S = 0x1F;
	static const short D = 0x20;
	static const short F = 0x21;
	static const short G = 0x22;
	static const short H = 0x23;
	static const short J = 0x24;
	static const short K = 0x25;
	static const short L = 0x26;
	/*
	static const short :   ;	=0x27;
	static const short ��  ��	=0x28;*/
	static const short Enter = 0x1C;
	static const short L_SHIFT = 0x2A;
	static const short Z = 0x2C;
	static const short X = 0x2D;
	static const short C = 0x2E;
	static const short V = 0x2F;
	static const short B = 0x30;
	static const short N = 0x31;
	static const short M = 0x32;
	/*
	static const short <  ,	=0x33;
	static const short >  .	=0x34;
	static const short ?  /	=0x35;*/
	static const short R_SHIFT = 0x36;
	static const short L_CTRL = 0x1D;
	static const short L_ALT = 0x38;
	static const short SpaceBar = 0x39;
	static const short R_ALT = (short)0xE038;
	static const short R_CTRL = (short)0xE01D;
	static const short Insert =/*E0*/ 0x52;
	static const short Delete =/*E0*/ 0x53;
	static const short Left_Arrow = (short)0xE04B;
	static const short Home = (short)0xE047;
	static const short End = (short)0xE04F;
	static const short Up_Arrow = (short)0xE048;
	static const short Dn_Arrow = (short)0xE050;
	static const short Page_Up = (short)0xE049;
	static const short Page_Down = (short)0xE051;
	static const short Right_Arrow = (short)0xE04D;
	static const short NumLock = 0x45;
	static const short Numeric_7 = 0x47;
	static const short Numeric_4 = 0x4B;
	static const short Numeric_1 = 0x4F;
	//static const short Numeric_/	=0xNo;te 3
	static const short Numeric_8 = 0x48;
	static const short Numeric_5 = 0x4C;
	static const short Numeric_2 = 0x50;
	static const short Numeric_0 = 0x52;
	//static const short Numeric_*	=0x37;    // *
	static const short Numeric_9 = 0x49;
	static const short Numeric_6 = 0x4D;
	static const short Numeric_3 = 0x51;
	/*
	static const short Numeric_.	=0x53;    // .
	static const short Numeric_-	=0x4A;    // -
	static const short Numeric_+	=0x4E;    // +
	*/
	//static const short DO NOT USE	=(short)0xE07E;
	static const short NumericEnter = (short)0xE01C;
	static const short Esc = 0x1;
	static const short F1 = 0x3B;
	static const short F2 = 0x3C;
	static const short F3 = 0x3D;
	static const short F4 = 0x3E;
	static const short F5 = 0x3F;
	static const short F6 = 0x40;
	static const short F7 = 0x41;
	static const short F8 = 0x42;
	static const short F9 = 0x43;
	static const short F10 = 0x44;
	static const short F11 = 0x57;
	static const short F12 = 0x58;
	//static const short PrintScreen	Note 4
	static const short ScrollLock = 0x46;
	//static const short Pause			Note 5
	static const short Left_Win = (short)0xE05B;
	static const short Right_Win = (short)0xE05C;
	static const short Application = (short)0xE05D;
	static const short ACPI_Power = (short)0xE05E;
	static const short ACPI_Sleep = (short)0xE05F;
	static const short ACPI_Wake = (short)0xE063;
	static const short DBE_KATAKANA = 0x70;
	static const short DBE_SBCSCHAR = 0x77;
	static const short CONVERT = 0x79;
	static const short NONCONVERT = 0x7B;
};