Внешняя компонента для 1С общего назначения по технологии Native на C++ в формате Visual Studio 2015. 
Доступные функции:
    PID() - Число - получает идентификатор текущего процесса
    Sleep(Число КоличествоМилисекунд) - выполняет паузу
    IsAdmin() - Булево - получает признак наличия административных привилегий
    GetCaretPos() - запоминает позицию каретки
    MoveWindowToCaretPos() - перемещает левый верхний угол окна в запомненную позицию каретки
    Run(Строка ИсполняемыйФайл, Строка ПараметрыЗапуска, Строка ТекущийКаталог, Булево ОжидатьЗавершения, Булево Элевация) - выполняет команду системы
