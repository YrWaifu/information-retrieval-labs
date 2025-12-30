@echo off
REM Простой скрипт для компиляции LaTeX документа в PDF на Windows
REM Использование: просто запустите compile.bat

set filename=report

echo Компиляция %filename%.tex в PDF...
pdflatex %filename%.tex

if %errorlevel% equ 0 (
    echo.
    echo Компиляция успешно завершена! PDF файл: %filename%.pdf
) else (
    echo.
    echo ОШИБКА: Компиляция не удалась. Проверьте, установлен ли LaTeX (MiKTeX или TeX Live).
    echo.
    echo Для установки LaTeX:
    echo 1. MiKTeX: https://miktex.org/download
    echo 2. TeX Live: https://www.tug.org/texlive/windows.html
)

pause

