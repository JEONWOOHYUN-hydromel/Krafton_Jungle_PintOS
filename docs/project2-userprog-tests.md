# Project 2 Userprog Tests Quick Map

`pintos/tests/userprog` 테스트가 대략 무엇을 확인하는지 정리한 문서다. 구현 방법은 일부러 자세히 적지 않는다.

## Argument Passing

| 테스트 | 확인 내용 |
| --- | --- |
| `args-none` | 인자 없는 실행의 `argc/argv` |
| `args-single` | 인자 1개 전달 |
| `args-multiple` | 여러 인자 전달 |
| `args-many` | 많은 인자 전달 |
| `args-dbl-space` | 연속 공백 처리 |

## Basic Syscall

| 테스트 | 확인 내용 |
| --- | --- |
| `halt` | `halt()`가 머신을 종료하는지 |
| `exit` | `exit(status)`와 종료 메시지 |

## Exec / Wait

| 테스트 | 확인 내용 |
| --- | --- |
| `exec-once` | 자식 프로그램 1회 실행 |
| `exec-arg` | `exec()`로 실행한 자식의 argument passing |
| `exec-missing` | 없는 파일 실행 실패 |
| `exec-bad-ptr` | 잘못된 exec 문자열 포인터 |
| `exec-boundary` | 페이지 경계의 exec 문자열 |
| `exec-read` | 실행 파일과 읽기 동작 |
| `wait-simple` | 자식 exit status 회수 |
| `wait-twice` | 같은 자식에 대한 중복 wait 실패 |
| `wait-bad-pid` | 자식이 아닌 pid wait 실패 |
| `wait-killed` | 커널에 의해 죽은 자식의 status |

## File Creation / Removal

| 테스트 | 확인 내용 |
| --- | --- |
| `create-normal` | 정상 파일 생성 |
| `create-empty` | 빈 이름 처리 |
| `create-null` | NULL 포인터 처리 |
| `create-bad-ptr` | 잘못된 파일명 포인터 |
| `create-bound` | 페이지 경계 파일명 |
| `create-exists` | 이미 있는 파일 생성 |
| `create-long` | 긴 파일명 |
| `remove` 계열 | 파일 삭제 동작 |

## Open / Close

| 테스트 | 확인 내용 |
| --- | --- |
| `open-normal` | 정상 open과 fd 반환 |
| `open-missing` | 없는 파일 open 실패 |
| `open-empty` | 빈 이름 open |
| `open-null` | NULL 포인터 |
| `open-bad-ptr` | 잘못된 파일명 포인터 |
| `open-boundary` | 페이지 경계 파일명 |
| `open-twice` | 같은 파일 여러 번 open |
| `close-normal` | 정상 close |
| `close-twice` | 같은 fd 중복 close |
| `close-bad-fd` | 잘못된 fd close |

## Read / Write

| 테스트 | 확인 내용 |
| --- | --- |
| `read-normal` | 파일 read |
| `read-zero` | size 0 read |
| `read-bad-fd` | 잘못된 fd read |
| `read-bad-ptr` | 잘못된 buffer 포인터 |
| `read-boundary` | 페이지 경계 buffer |
| `read-stdout` | stdout에서 read 시도 |
| `write-normal` | 파일 또는 stdout write |
| `write-zero` | size 0 write |
| `write-bad-fd` | 잘못된 fd write |
| `write-bad-ptr` | 잘못된 buffer 포인터 |
| `write-boundary` | 페이지 경계 buffer |
| `write-stdin` | stdin에 write 시도 |

## Bad User Memory

| 테스트 | 확인 내용 |
| --- | --- |
| `bad-read` | 유저가 잘못된 주소를 읽음 |
| `bad-read2` | 다른 형태의 잘못된 read |
| `bad-write` | 유저가 잘못된 주소에 씀 |
| `bad-write2` | 다른 형태의 잘못된 write |
| `bad-jump` | 잘못된 주소로 jump |
| `bad-jump2` | 다른 형태의 잘못된 jump |
| `boundary` helpers | 페이지 경계 검증에 쓰이는 보조 코드 |

## Fork

| 테스트 | 확인 내용 |
| --- | --- |
| `fork-once` | 기본 fork |
| `fork-multiple` | 여러 번 fork |
| `fork-recursive` | 재귀 fork |
| `fork-read` | fork 후 메모리/파일 읽기 |
| `fork-close` | fork 후 fd close 관계 |
| `fork-boundary` | 페이지 경계 인자 |

## File Descriptor Inheritance / Multi Process

| 테스트 | 확인 내용 |
| --- | --- |
| `multi-child-fd` | 자식 프로세스와 fd 관계 |
| `multi-recurse` | 여러 프로세스 생성과 종료 |
| `no-vm/multi-oom` | 반복 생성 중 자원 부족 처리 |

## Read-Only Executable

| 테스트 | 확인 내용 |
| --- | --- |
| `rox-simple` | 실행 중인 파일 write 차단 |
| `rox-child` | 자식 실행 파일 write 차단 |
| `rox-multichild` | 여러 자식의 실행 파일 write 차단 |

## Child Helper Programs

| 파일 | 역할 |
| --- | --- |
| `child-simple` | exit status를 반환하는 기본 자식 |
| `child-bad` | 비정상 종료 자식 |
| `child-read` | 자식 read 동작 |
| `child-close` | 자식 close 동작 |
| `child-rox` | rox 테스트 보조 |

