program RP {
	version V1 {
		int write(string message) = 231;
		string read() = 232;
		int ping() = 233;
	} = 17;
} = 0x20000004;
