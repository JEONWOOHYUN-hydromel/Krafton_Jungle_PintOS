# Project 2 Filesystem Syscall Map

이 문서는 `userprog/syscall.c`에서 fd 관련 syscall을 구현할 때 연결해야 하는 PintOS filesystem API를 정리한다.

## 기본 헤더

```c
#include "filesys/filesys.h"   // filesys_create, filesys_open, filesys_remove
#include "filesys/file.h"      // file_read, file_write, file_close, ...
#include "devices/input.h"     // input_getc
#include <stdio.h>             // putbuf
```

파일 시스템 동시 접근 보호용으로 `syscall.c`에 전역 lock을 두는 것이 좋다.

```c
static struct lock filesys_lock;

void
syscall_init (void) {
    lock_init (&filesys_lock);
    ...
}
```

## fd Table

각 프로세스별 열린 파일 목록이 필요하다. 보통 `struct thread`에 둔다.

```c
#define FD_MAX 128

struct file *fd_table[FD_MAX];
int next_fd;
```

초기화 위치: `init_thread()`

```c
for (int i = 0; i < FD_MAX; i++)
    t->fd_table[i] = NULL;
t->next_fd = 2;
```

fd 의미:

| fd | 의미 |
| --- | --- |
| `0` | standard input |
| `1` | standard output |
| `2...` | opened files |

## Syscall 연결표

| syscall | 주요 처리 | 사용할 API |
| --- | --- | --- |
| `create(file, size)` | 파일 생성 | `filesys_create(file, size)` |
| `remove(file)` | 파일 삭제 | `filesys_remove(file)` |
| `open(file)` | 파일 열고 fd 배정 | `filesys_open(file)` |
| `filesize(fd)` | 파일 길이 반환 | `file_length(file)` |
| `read(fd, buffer, size)` | stdin 또는 파일에서 읽기 | `input_getc()`, `file_read(file, buffer, size)` |
| `write(fd, buffer, size)` | stdout 또는 파일에 쓰기 | `putbuf(buffer, size)`, `file_write(file, buffer, size)` |
| `seek(fd, pos)` | 파일 offset 이동 | `file_seek(file, pos)` |
| `tell(fd)` | 현재 파일 offset 반환 | `file_tell(file)` |
| `close(fd)` | 파일 닫고 fd 해제 | `file_close(file)` |

## 구현 흐름

`open(file)`:

```c
lock_acquire (&filesys_lock);
struct file *file_obj = filesys_open (file);
lock_release (&filesys_lock);

if (file_obj == NULL)
    return -1;

int fd = allocate_fd (file_obj);
return fd;
```

`read(fd, buffer, size)`:

```c
if (fd == 0) {
    for each byte:
        buffer[i] = input_getc ();
    return size;
}

struct file *file = get_file (fd);
if (file == NULL)
    return -1;

lock_acquire (&filesys_lock);
int ret = file_read (file, buffer, size);
lock_release (&filesys_lock);
return ret;
```

`write(fd, buffer, size)`:

```c
if (fd == 1) {
    putbuf (buffer, size);
    return size;
}

struct file *file = get_file (fd);
if (file == NULL)
    return -1;

lock_acquire (&filesys_lock);
int ret = file_write (file, buffer, size);
lock_release (&filesys_lock);
return ret;
```

`close(fd)`:

```c
struct file *file = get_file (fd);
if (file == NULL)
    return;

lock_acquire (&filesys_lock);
file_close (file);
lock_release (&filesys_lock);

thread_current ()->fd_table[fd] = NULL;
```

## 종료 시 정리

프로세스가 종료될 때 열린 fd를 모두 닫아야 한다. 위치는 `process_exit()`가 적절하다.

```c
for (int fd = 2; fd < FD_MAX; fd++) {
    if (curr->fd_table[fd] != NULL) {
        file_close (curr->fd_table[fd]);
        curr->fd_table[fd] = NULL;
    }
}
```

## 주의할 점

- 유저 포인터 검증은 file API 호출 전에 해야 한다.
- `fd == 0`은 `read`에서만 특별 처리한다.
- `fd == 1`은 `write`에서만 특별 처리한다.
- invalid fd는 보통 `-1` 반환 또는 no-op 처리한다. `close`는 반환값이 없다.
- `filesys_*`와 `file_*` 호출은 lock으로 감싸는 편이 안전하다.
- `exec` 중 실행 파일에 write를 막으려면 `file_deny_write()`와 `file_allow_write()` 흐름이 별도로 필요하다.
