# SysMonitor
C++ 시스템 & 프로그램 모니터링 도구.
실시간 하드웨어 지표 수집 · 프로세스 분석 · 이상 감지 · 세션 리포트 생성까지.
SysMonitor는 Windows 작업관리자에서 영감을 받은 C++ 포트폴리오 프로젝트입니다. 단순 수치 표시를 넘어 특정 프로그램을 타겟으로 추적하고, 이상 패턴을 감지해 알림을 보내며, 세션 단위 성능 리포트를 생성하는 것을 목표로 합니다.
## 주요 기능
* 시스템 모니터링
  * CPU · 메모리 · 디스크 I/O · 네트워크 송수신 실시간 수집
  * 코어별 사용률 · 프로세스 목록
* 이상 감지 & 알림
  * CPU/메모리 임계값 초과 감지
  * 이동평균 기반 스파이크 탐지
* 프로그램 타겟 추적
  * PID / 이름으로 핀고정
  * 프로세스별 CPU/메모리 히스토리
  * 자식 프로세스 추적
  * 프로세스별 네트워크 사용량
* 행동 분석
  * 파일 접근 감시
  * 네트워크 연결 목록
  * DLL 모듈 목록
* 세션 리포트
  * 실행 세션별 JSON 리포트
  * 메모리 증가 추세 감지
  * 실행할 때마다 성능 비교
## 아키텍쳐
<img width="2564" height="1128" alt="image" src="https://github.com/user-attachments/assets/4e316932-74fb-4b7b-a6db-c03f6c648269" />


## 설계
### /collectors
 SystemCollector.cpp - 전체 CPU, 메모리, 디스크, 네트워크 검사 후 DataStore에 저장
- saveSystemScan() -> DataStroe에 저장
- getCpuUsage()
- getMemoryUsage()
- getDiskUsage()
- getNetworkUsage()

 ProcessCollector.cpp - 전체 프로세스의 현재 각 CPU, 메모리, 디스크, 네트워크 검사 후 
사용 후 Handle close
- saveProcessScan() -> DataStore에 저장
- getProcessCpu()
- getProcessMemory()
- getProcessDisk()
- getProcessNetwork()

 TargetProgramCollector.cpp - 타겟 프로세스 CPU, 메모리, 디스크, 네트워크 검사, 자식프로세스 확인
  Handel 계속 열어둠
- saveTargetScan() -> ProgramTracker에 전달
- getProcessCpu()
- getProcessMemory()
- getProcessDisk()
- getProcessNetwork()
- getChildTree()
- getConnections() -> 연결 IP
- getOpenFiles() -> 접근 파일

### /tracker
 ProgramTracker.cpp - 타겟 프로세스 히스토리 저장, 추세 분석, 이상 감지, 세션 리포트
  - cpuHitory, memHistory, diskHistory, networkHistory
  - isLeaking() -> 추세분석
  - hasSpike() -> AlertEngine -> 이상 감지
  - reportSesion -> SessionReporter 호출
  SessionReporter.cpp - 세션 리포트 json으로 저장

### /datastore
 DataStore.cpp - SystemHistory, ProcessHistory, targets 소유
  - pinTarget()
  - unpinTarget()
  - hasSysProcSpike() -> System, Process 이상 감지

### /alert
 AlertEngine.cpp
 Notifier.cpp

### 사용 라이브러리
 로깅 - spdlog
 세션 리포트 - nlohmann/json
 알림(Toast) - WinToastLib
 UI & Graph - Dear ImGui, ImPlot

##
Process Collector
 - 상위 10개의 프로세스 정보 수집
   * PID
   * 프로세스 이름
   * CPU
   * 전용 메모리 크기(Private Mem)
   * 실제 물리 메모리 크기 (Working Set)
Target Program Collector
 - 최대 5개의 특정 프로세스 정보 수집
   * PID
   * 프로세스 이름
   * CPU (0.5 초)
   * 전용 메모리 크기
   * 실제 물리 메모리 크기
   * 핸들 정보
   * 네트워크 (트래픽 확인 0.5초)
 Data Store
  - 링 버퍼 형식으로 프로그램 내부에 저장 -> Session Reporter 이전의 임시 저장소
   * 크기는 8MB 블록 저장
   * 시스템, 일반 프로세스는 상위 10개, 타겟 프로그램의 경우 꺼져 있는것까지 최대 5개
   * 삭제된 정보는 Session Reporter를 통해 조회
 Session Reporter
  - 시계열(Time Stamp)로 매트릭스 별로 저장 > DuckDB
   * 1분 주기로 저장, Session Reporter에 저장된 Data Store 블록은 삭제
   * 매트릭스
        > System(Time Stamp + 메트리스 이름)
           - CPU Total
           - Processor Queue Length 병목 판단용
           - Kernel Time 이상 탐지용
           - User Time   이상 탐지용
           - 디스크 쓰기
           - 디스크 읽기
           - 커밋된 메모리
           - 사용중인 메모리
           - 네트워크 읽기
           - 네트워크 쓰기
        > Process(Time Stamp + PID + 프로세스 이름 + 메트릭스 이름)
           - CPU
           - 전용 메모리 크기(Private Memorry)
           - 실제 물리 메모리 크기(Working Set)
       > Target Program ( Time Stamp + PID + 프로세스 이름 + 메트릭스 이름)
           - CPU
           - 전용 메모리 크기(Private Memorry)
           - 실제 물리 메모리 크기(Working Set)
           - Network
           - 핸들


 

d
