# [SWJunlge] Week06 Proxy Lab
SW사관학교 정글 6주차 C언어로 웹서버 구현 과제 리포지토리입니다. 아래는 과제 제출 시 Pull Request 입니다.

## 과제 제출 및 결과
```shell
*** Basic ***
Starting tiny on 23209
Starting proxy on 20501
1: home.html
   Fetching ./tiny/home.html into ./.proxy using the proxy
   Fetching ./tiny/home.html into ./.noproxy directly from Tiny
   Comparing the two files
   Success: Files are identical.
2: csapp.c
   Fetching ./tiny/csapp.c into ./.proxy using the proxy
   Fetching ./tiny/csapp.c into ./.noproxy directly from Tiny
   Comparing the two files
   Success: Files are identical.
3: tiny.c
   Fetching ./tiny/tiny.c into ./.proxy using the proxy
   Fetching ./tiny/tiny.c into ./.noproxy directly from Tiny
   Comparing the two files
   Success: Files are identical.
4: godzilla.jpg
   Fetching ./tiny/godzilla.jpg into ./.proxy using the proxy
   Fetching ./tiny/godzilla.jpg into ./.noproxy directly from Tiny
   Comparing the two files
   Success: Files are identical.
5: tiny
   Fetching ./tiny/tiny into ./.proxy using the proxy
   Fetching ./tiny/tiny into ./.noproxy directly from Tiny
   Comparing the two files
   Success: Files are identical.
Killing tiny and proxy
basicScore: 40/40

*** Concurrency ***
Starting tiny on port 3208
Starting proxy on port 13863
Starting the blocking NOP server on port 8826
Trying to fetch a file from the blocking nop-server
Fetching ./tiny/home.html into ./.noproxy directly from Tiny
Fetching ./tiny/home.html into ./.proxy using the proxy
Checking whether the proxy fetch succeeded
Success: Was able to fetch tiny/home.html from the proxy.
Killing tiny, proxy, and nop-server
concurrencyScore: 15/15

*** Cache ***
Starting tiny on port 22753
Starting proxy on port 21651
Fetching ./tiny/tiny.c into ./.proxy using the proxy
Fetching ./tiny/home.html into ./.proxy using the proxy
Fetching ./tiny/csapp.c into ./.proxy using the proxy
Killing tiny
Fetching a cached copy of ./tiny/home.html into ./.noproxy
Success: Was able to fetch tiny/home.html from the cache.
Killing proxy
cacheScore: 15/15

totalScore: 70/70
```
## Tiny Web Server 구현
* 요청 라인을 읽고 분석해 정적/동적 컨텐츠를 응답합니다.
* 파일을 읽을 수 없는 등 오류가 발생한 경우 에러 페이지를 응답합니다.

## Web Proxy 구현
#### 클라이언트의 요청을 받아 `ip`, `port`, `method`, `path` 읽기
- 동일 호스트가 dotted ingeter, hostname으로 표기될 수 있음을 처리하기 위해 `Getaddrinfo`, `Getnameinfo`를 사용해 모두 `uint32_t` 타입 ip주소로 변환하였습니다. (+사이즈 감소)
- 포트는 `uint16_t` 으로 변환하였습니다.
- method는 enum type을 정의하여 처리했습니다.
####  위 4개 정보 조합으로 cache list를 검색
- 캐시 리스트는 이중 연결 리스트로 구현했습니다. (삽입, 삭제가 상수시간)
- 캐시 데이터가 있는 경우 즉시 클라이언트로 전달하고 리턴합니다.
- 캐시 데이터가 없는 경우 서버로 요청을 전달합니다.
####  서버에서 응답을 받아 클라이언트로 전달
- 헤드의 경우 한 줄씩 클라이언트에 전달하고, 동시에 `cache_data` 에 씁니다.
- `cache_data`는 `MAX_OBJECT_SIZE` 로 동적할당된 메모리입니다.
- 읽을 내용이 `MAX_OBJECT_SIZE`를 초과하는 경우 1을 리턴해 캐시 대상에서 제외합니다.
- 헤드에서 content-length의 값을 읽어와 길이만큼 한번에 읽고 클라이언트에 전달합니다. 역시 동시에 캐시에 씁니다.
- 캐시에 쓴 데이터의 시작주소, 캐시에 실제 쓰여진 길이를 따로 저장합니다.
####  응답을 캐시에 저장
- 만약 캐시 사이즈가 `MAX_CACHE_SIZE`에 도달하면, 저장 가능할 때까지 사용된 지 가장 오랜 시간이 지난 캐시 (LRU)를 반복하여 삭제합니다.
* 세마포어를 이용해 캐시 접근을 제어합니다.

## 테스트 영상
[드라이브에서 보기](https://drive.google.com/file/d/1HHiAiBIeXjLy8yuZi9HmGv_FwDZHbZsM/view?usp=sharing) 

## 향후 보완사항
* 가능하다면 HEAD method를 사용해 head와 content를 구분, content만 캐시하도록 구현하고 싶습니다.

---


####################################################################
# CS:APP Proxy Lab
#
# Student Source Files
####################################################################

This directory contains the files you will need for the CS:APP Proxy
Lab.

proxy.c
csapp.h
csapp.c
    These are starter files.  csapp.c and csapp.h are described in
    your textbook. 

    You may make any changes you like to these files.  And you may
    create and handin any additional files you like.

    Please use `port-for-user.pl' or 'free-port.sh' to generate
    unique ports for your proxy or tiny server. 

Makefile
    This is the makefile that builds the proxy program.  Type "make"
    to build your solution, or "make clean" followed by "make" for a
    fresh build. 

    Type "make handin" to create the tarfile that you will be handing
    in. You can modify it any way you like. Your instructor will use your
    Makefile to build your proxy from source.

port-for-user.pl
    Generates a random port for a particular user
    usage: ./port-for-user.pl <userID>

free-port.sh
    Handy script that identifies an unused TCP port that you can use
    for your proxy or tiny. 
    usage: ./free-port.sh

driver.sh
    The autograder for Basic, Concurrency, and Cache.        
    usage: ./driver.sh

nop-server.py
     helper for the autograder.         

tiny
    Tiny Web server from the CS:APP text

