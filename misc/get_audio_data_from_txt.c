#include <stdio.h>
#include <string.h>
//#define INPUT_FILE_NAME "audio.txt"
#define INPUT_FILE_NAME "uart.txt"
#define OUT_FILE_NAME "pcmdump"

static char temp_buf[100];


void main(int argc, char* argv[]) {
	int readbytes;
	int i;
	short value = 0;
	int sign = 0;
	int get_one_int = 0;
	char infile[20];
	char outfile[20];

	memset(infile, 0, 20);
	memset(outfile, 0, 20);

	if (argc < 3) {
		printf("usage: ./test infile outfile\n");
		memcpy(infile, INPUT_FILE_NAME, 9);
		memcpy(outfile, OUT_FILE_NAME, 7);
	} else {
		/**/
		printf("[argv1:%s] (%d), [argv2:%s] (%d)\n", argv[1], strlen(argv[1]), argv[2], strlen(argv[2]));
		memcpy(infile, argv[1], strlen(argv[1]));
		memcpy(outfile, argv[2], strlen(argv[2]));
	}

	printf("infile:%s, outfile:%s\n", infile, outfile);

	FILE *handle = fopen(infile, "r");
	if (handle == NULL) {
        printf("Encountered error reading %s\n", infile);
		return;
	}

	FILE *handle_out = fopen(outfile, "wb");
	if (handle_out == NULL) {
        printf("Encountered error writing %s\n", outfile);
		return;
	}
	

	// 方法一:
	// 通过一次读取一段buffer,来手动算出int
	// 空格为每一个int的分割
	// ASCII <数字 0-9> <ASCII 48-57>
	// ASCII <减号 -> <ASCII 45>
	#if 1
	while(!feof(handle)) {
		readbytes = fread(temp_buf, 1, 100, handle);
		
		/*filter int*/
		for (i = 0; i < readbytes; i++) {
			
			// 负数
			if (temp_buf[i] == 45)
				sign =1;
			// 单个数字
			else if (temp_buf[i] >= 48 && temp_buf[i] <= 57) {
				get_one_int = 1;
				value = value * 10 + (temp_buf[i] -48);
			}
			//空格或其他字符,一个int的结束
			else {
				value = sign? (0 - value) : value;
				
				// save int if have get one int
				if (get_one_int) {
					//printf("%d \n", value);
					fwrite(&value, sizeof(value), 1, handle_out);
					get_one_int = 0;
				}

				// clear value for next int
				value = 0;
				sign = 0;
			}
		}
	}
	goto exit;
	#endif
	
	//方法二:
	//通过系统函数来提取int
	do {
		/*read short int from file*/
		fscanf(handle, "%hd", &value);
		fwrite(&value, sizeof(value), 1, handle_out);
		//printf("%d ", value);
	} while( !feof(handle));

exit:
	fclose(handle);
	fclose(handle_out);
}
