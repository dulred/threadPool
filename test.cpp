#include <cstring>  // 包含 strcpy 函数头文件
#include <iostream>

#define ARRAY_NUM(arr) (sizeof(arr)/sizeof(arr[0]))

template <typename T , size_t N>
void reverseArr(T arr[])
{   
    T* p = arr;
    T* q = arr + N - 1;

    while (p < q)
    {
        T tmp = *p;
        *p = *q;
        *q = tmp;
        p++;
        q--;
    }
    
}

int main(int argc, char const *argv[])
{
    // int arr[6] = {1,2,3,8,9,6};
    // char arr[9]  = "helloqqb";

    // reverseArr<char,ARRAY_NUM(arr)>(arr);
    
    // for (int i = 0; i < ARRAY_NUM(arr); i++)
    // {
    //     std::cout << arr[i] << std::endl;
    // }

    

    return 0;
}

