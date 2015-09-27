program RP {
	version V1 {
		int write(string message) = 231;
		string read() = 232;
		int ping() = 233;
		int replicate_write(string message) = 234;
		int replicate_read() = 235;
	} = 17;
} = 0x20000004;
