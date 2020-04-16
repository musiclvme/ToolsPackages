#include <stdio.h>

#define INPUT_FILE_NAME "audio.txt"
#define OUT_FILE_NAME "pcmdump"

static char temp_buf[100];


void main() {
	int readbytes;
	int i;
	short value = 0;
	int sign = 0;
	int get_one_int = 0;
	FILE *handle = fopen(INPUT_FILE_NAME, "r");
	if (handle == NULL) {
        printf("Encountered error reading %s\n", INPUT_FILE_NAME);
		return;
	}

	FILE *handle_out = fopen(OUT_FILE_NAME, "wb");
	if (handle_out == NULL) {
        printf("Encountered error writing %s\n", OUT_FILE_NAME);
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
