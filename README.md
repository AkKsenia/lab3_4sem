# multi_threaded_data_handler

Программа демонстрирует параллельную обработку данных с использованием потоков и мьютексов:

- Выделяются три группы потоков: одна для выделения памяти, одна для заполнения памяти значениями и одна для сохранения содержимого памяти в файл.
- Каждая группа потоков использует мьютекс для синхронизации доступа к общим ресурсам.
- Программа использует пользовательские функции выделения памяти my_malloc и my_free.
