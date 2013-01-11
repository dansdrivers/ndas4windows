@if "%1" equ "" (
@echo "ProductVersion must be specified. (e.g. 3.31.1701)"
@goto :eof
)

set PRODUCT_VERSION=%1

call .\build_setup /p:sku=ndasscsi /p:platform=i386 /p:ProductVersion=%PRODUCT_VERSION%
call .\build_setup /p:sku=ndasscsi /p:platform=amd64 /p:ProductVersion=%PRODUCT_VERSION%
