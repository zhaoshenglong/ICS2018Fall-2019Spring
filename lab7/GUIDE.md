#Lab 7 Memory Allocator
## Overview
This lab requires not only correctness but both space and time performance.</br>
Most of the code are the same as what CMU teachers demonstrated in their book, and it really count that you unsdertand every line of the code.</br> 
Only use segregated free list is enough to get a good score</br>
You can also use some datastructure like AVL / RB Tree
## My feeling
### Debug
Debug is difficult in this lab.</br>
1. First, you should confirm that there is no bugs in your high level design.
2. Second, insert ``printf()`` to see where is wrong.
3. Alternatively, you can use some tools like gdb, or you can read chapter 7 first. In chapter 7, you will find some tricks. ``gcc -g`` will help you.
### Performance
#### Implicit free list
It seems that if you use implicit free list, you will get about 60 score.</br>
That means you won't get 0 provided you do work on this lab.
</br>
#### Explicit free list
I don't know how much score you will get if using this schema, but will mare than 60.
</br>
#### Segregated free list
If you simply use this schema, you will get about 85 score. </br>
* 4 traces will worsen your util. ``binary``,``binary2``,``realloc``,``realloc2``.
* There are common points in these traces and you can make use of that.
* Common block is ``4072``, ``72``, ``512``,``128``. 
#### AVL / other trees
You can also use data structures like AVL / RB tree.
Here is a blog from whom got full score [CSAPP:lab7](https://blog.csdn.net/huang1024rui/article/details/50526998)
</br>
There are some other PPT/PDF you can find on the internet, ics is a common course in many school and if have any question, you can refer to that or ask your assistant.
## Score
Performance: 98/100 </br>
Final score: [TODO]
