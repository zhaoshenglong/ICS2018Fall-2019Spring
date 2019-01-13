#include <stdio.h>
void bubble_b(long *data, long count){
	long *i, *last = data + count -1;
	for(; last != data; last--){
		for(i = data; i != last; i++){
			if(*i > *(i + 1)){
				long t = *(i+1);
				*(i+1) = *i;
				*i = t;
			}
		}
	}	
}

void main(){
	long data[] = {5,6,8,2,3,1,7};
	bubble_b(data, 7);
	for(int i = 0; i < 7; i++){
		printf("%d ",data[i]);
	}
}
