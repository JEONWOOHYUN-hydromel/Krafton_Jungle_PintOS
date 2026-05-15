# Project 2 System Call Summary

출처: KAIST PintOS Project 2 System Calls 문서  
https://casys-kaist.github.io/pintos-kaist/project2/system_call.html

이 문서는 시스템콜 문서의 핵심 내용을 한국어로 간단히 정리한다. 구현 방법보다는 각 syscall이 어떤 의미인지에 초점을 둔다.

## 시스템콜 기본 구조

유저 프로그램은 `syscall` 명령으로 커널에 요청한다.

레지스터 사용:

| 레지스터 | 의미 |
| --- | --- |
| `%rax` | syscall 번호 |
| `%rdi` | 1번째 인자 |
| `%rsi` | 2번째 인자 |
| `%rdx` | 3번째 인자 |
| `%r10` | 4번째 인자 |
| `%r8` | 5번째 인자 |
| `%r9` | 6번째 인자 |

반환값은 `struct intr_frame`의 `R.rax`에 넣는다.

```c
f->R.rax = return_value;
```

## 구현해야 하는 syscall

### `halt`

```c
void halt (void);
```

Pintos를 종료한다. 내부적으로 `power_off()`를 호출한다. 일반 테스트에서는 자주 쓰이지 않는다.

### `exit`

```c
void exit (int status);
```

현재 유저 프로그램을 종료한다. `status`는 부모가 `wait()`할 때 받을 종료 상태다.

관례:

| status | 의미 |
| --- | --- |
| `0` | 성공 |
| nonzero | 오류 |

### `fork`

```c
pid_t fork (const char *thread_name);
```

현재 프로세스를 복제해서 새 프로세스를 만든다. 자식은 부모의 리소스를 복사해야 한다.

중요한 점:

- 부모에게는 자식 pid를 반환한다.
- 자식에게는 `0`을 반환한다.
- 자식 복제가 성공했는지 부모가 알기 전까지 부모가 먼저 반환하면 안 된다.
- 파일 디스크립터와 유저 메모리 공간도 복제 대상이다.

### `exec`

```c
int exec (const char *cmd_line);
```

현재 프로세스를 `cmd_line`에 적힌 실행 파일로 바꾼다. 인자도 함께 전달한다.

중요한 점:

- 성공하면 원래 흐름으로 돌아오지 않는다.
- 로드나 실행에 실패하면 종료 상태 `-1`로 종료한다.
- `exec`를 호출한 스레드 이름은 바꾸지 않는다.
- 열린 file descriptor는 `exec` 이후에도 유지된다.

### `wait`

```c
int wait (pid_t pid);
```

자식 프로세스가 종료될 때까지 기다리고, 자식의 exit status를 반환한다.

반환 규칙:

| 상황 | 반환 |
| --- | --- |
| 자식이 `exit(status)`로 종료 | `status` |
| 자식이 커널에 의해 죽음 | `-1` |
| pid가 직접 자식이 아님 | `-1` |
| 같은 자식을 이미 wait함 | `-1` |

주의:

- 이미 죽은 자식도 부모가 status를 회수할 수 있어야 한다.
- 부모가 자식을 wait하지 않고 종료해도 리소스는 해제되어야 한다.
- 자식이 부모보다 먼저 죽거나, 부모가 자식보다 먼저 죽는 경우 모두 고려해야 한다.
- 최초 프로세스가 종료될 때까지 Pintos가 끝나면 안 된다.

### `create`

```c
bool create (const char *file, unsigned initial_size);
```

새 파일을 만든다. 성공하면 `true`, 실패하면 `false`.

주의:

- 파일을 만드는 것과 여는 것은 별개다.
- 생성 후 사용하려면 `open()`을 따로 해야 한다.

### `remove`

```c
bool remove (const char *file);
```

파일을 삭제한다. 성공하면 `true`, 실패하면 `false`.

주의:

- 파일이 열려 있어도 삭제할 수 있다.
- 열려 있는 파일을 삭제해도 그 fd가 자동으로 닫히지는 않는다.

### `open`

```c
int open (const char *file);
```

파일을 열고 fd를 반환한다. 실패하면 `-1`.

fd 규칙:

| fd | 의미 |
| --- | --- |
| `0` | stdin |
| `1` | stdout |
| `2...` | 일반 파일 |

주의:

- `open()`은 fd `0`, `1`을 반환하면 안 된다.
- 각 프로세스는 독립적인 fd 집합을 가진다.
- 같은 파일을 여러 번 열면 매번 새로운 fd가 생긴다.
- 서로 다른 fd는 독립적으로 close된다.
- child process는 fd를 상속한다.

### `filesize`

```c
int filesize (int fd);
```

열린 파일 fd의 크기를 byte 단위로 반환한다.

### `read`

```c
int read (int fd, void *buffer, unsigned size);
```

fd에서 `size` byte를 읽어 `buffer`에 저장한다.

반환:

| 상황 | 반환 |
| --- | --- |
| 실제 읽은 byte 수 | `>= 0` |
| EOF | `0` |
| 읽기 실패 | `-1` |

특수 fd:

- `fd == 0`: 키보드에서 `input_getc()`로 읽는다.

### `write`

```c
int write (int fd, const void *buffer, unsigned size);
```

`buffer`의 내용을 fd에 쓴다.

반환:

| 상황 | 반환 |
| --- | --- |
| 실제 쓴 byte 수 | `>= 0` |
| 쓸 수 없음 | `0` 또는 `-1`, 상황에 맞게 |

특수 fd:

- `fd == 1`: 콘솔에 출력한다.
- 콘솔 출력은 가능하면 `putbuf()` 한 번으로 처리한다.

주의:

- 기본 파일 시스템은 Project 4 전까지 파일 크기 확장을 지원하지 않는다.
- 파일 끝 이후 write는 제한될 수 있다.

### `seek`

```c
void seek (int fd, unsigned position);
```

파일의 다음 read/write 위치를 `position`으로 옮긴다.

주의:

- 파일 끝 이후로 seek하는 것 자체는 오류가 아니다.
- 이후 read는 `0` byte를 반환할 수 있다.
- 기본 PintOS 파일 시스템에서는 파일 확장이 아직 제한된다.

### `tell`

```c
unsigned tell (int fd);
```

현재 fd의 다음 read/write 위치를 반환한다.

### `close`

```c
void close (int fd);
```

fd를 닫는다.

주의:

- 프로세스가 종료될 때 열린 fd는 모두 닫혀야 한다.
- 종료 시 `close()`를 각 fd에 호출한 것과 같은 효과가 있어야 한다.

## 동기화

파일 시스템 코드는 여러 스레드가 동시에 호출해도 안전하지 않다. 따라서 파일 시스템 관련 syscall은 critical section으로 보호해야 한다.

보통 `syscall.c`에 lock을 둔다.

```c
static struct lock filesys_lock;
```

보호 대상 예:

- `filesys_create`
- `filesys_open`
- `filesys_remove`
- `file_read`
- `file_write`
- `file_close`
- `process_exec`에서 실행 파일을 여는 부분

## 안전성

유저 프로그램이 잘못된 syscall 인자를 넘겨도 커널이 panic/assert/fault로 죽으면 안 된다.

주의할 인자:

- NULL 포인터
- 커널 주소
- 유저 영역이지만 매핑되지 않은 주소
- 페이지 경계를 넘는 buffer
- null terminator가 없는 문자열
- invalid fd

잘못된 인자 처리 방식은 syscall마다 다를 수 있다.

가능한 처리:

- 오류값 반환
- 현재 프로세스 종료

중요한 원칙:

> 유저 프로그램이 OS 자체를 망가뜨릴 수 있으면 안 된다.

유저가 Pintos를 정상적으로 종료할 수 있는 유일한 방법은 `halt()` syscall이어야 한다.
