
playback:play_back.c
	arm-linux-gcc $^ -o $@ -lasound
	cp playback /home/xiaoming/My_Class/rootfs_study/rootfs/root/
clean:
	rm  playback






































