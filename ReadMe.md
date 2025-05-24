## cmake选项

1. 添加vcpkg路径：类似`-DCMAKE_TOOLCHAIN_FILE=C:\Users\yy572\.vcpkg-clion\vcpkg\scripts\buildsystems\vcpkg.cmake`

   - CLion安装vcpkg时会让你选择是否添加该选项

     <img src="./images/image-20250524164103208.png" alt="image-20250524164103208" style="zoom: 67%;" /> 

   - 后续添加只需手动复制即可

     <img src="./images/image-20250524164414397.png" alt="image-20250524164414397" style="zoom:50%;" />

2. Mingw额外选项：vcpkg默认下载的软件包是MSVC的版本，对于mingw应使用该选项进行切换：`-DVCPKG_TARGET_TRIPLET=x64-mingw-static`

   <img src="./images/image-20250524164641049.png" alt="image-20250524164641049" style="zoom:50%;" />