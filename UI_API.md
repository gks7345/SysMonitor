# 개요
API(C++) -> postman 시연 -> UI(React + CSS) 붙이기
DataStore의 RingBuffer에서 가져올 수 있는 정보는 RingBuffer에서 가져오기
## 보여줄 정보
1. 최근 120초 간의 수집 내용 실시간 그래프(현재 내용 포함) -> DataStore가 아닌 링버퍼에서 가져오기
	└── 이유: RingBuffer 용량이 120로 설정되어 있음 약 2분이 시스템 상채 추세를 파악하기에 적절
2. 실시간(현재의) 정보 -> 텍스트? 표? 형식
3. 과거 기록 날짜 선택 -> 해당 날짜의 SummaryStore DuckDB에 저장된 요약 정보를 기본 출력
4. 각 항목(시스템, 프로세스, 타겟 프로그램) -> 과거 날짜 선택 -> 요약이 아닌 DuckDB에 저장된 정보 출력

# 전체 구현 계획
## 기술 스택
### 백엔드(C++)
	cpp-httplib - HTTP 서버 (헤더 온리)
	nlohmann/json — JSON 직렬화
	기존 DataStore, RingBuffer, SessionReport 재활용

### 프론트 엔드(React)
	Vite + React - 빌드 도구
	Recharts - 그래프용/React 친화적
	Tailwind CSS - 스타일링
	axios 또는 fetch - API 호출

## 폴더 구조
```
SysMonitor/
├── src/ include/          ← 기존 C++ (서버 코드 추가)
├── CMakeLists.txt         ← cpp-httplib, nlohmann/json 추가
├── web/                   ← React 프로젝트
│   ├── src/
│   │   ├── components/
│   │   │   ├── SystemPanel.tsx     ← 시스템 정보 패널
│   │   │   ├── ProcessPanel.tsx    ← 프로세스 Top N
│   │   │   ├── TargetPanel.tsx     ← 타겟 프로그램 패널
│   │   │   └── HistoryPanel.tsx    ← 과거 기록 조회
│   │   ├── App.tsx
│   │   └── main.tsx
│   ├── dist/              ← 빌드 결과 (C++ 서버가 서빙)
│   └── package.json
└── SysMonitor.exe
```

## API 설계
### 실시간 (RingBuffer -> hot tier)
	GET /api/current
    → 현재 시스템/프로세스/타겟 스냅샷 1개
    → 1초마다 폴링

	GET /api/system/history?n=120
    → 최근 120초 시계열 (RingBuffer)
    → 실시간 그래프용

	GET /api/process/top?n=10
    → 현재 프로세스 Top N

	GET /api/targets
    → 타겟 목록 + 현재 상태 + 실시간 수치

### 과거 (DuckDB → cold tier)
    GET /api/history/summary?date=2026-06-27
    → SummaryStore에서 해당 날짜 요약
    → UI_API.md 3번 항목

    GET /api/history/system?from=&to=
    → sys_snapshot 시간범위 조회
    → UI_API.md 4번 항목 (시스템)

    GET /api/history/process?from=&to=&name=
    → proc_snapshot 시간범위 조회
    → UI_API.md 4번 항목 (프로세스)

    GET /api/history/target?from=&to=&name=
    → target_snapshot 시간범위 조회
    → UI_API.md 4번 항목 (타겟)

## 단계별 로드맵
### 1단계 — E2E MVP (C++ 서버 + React 기본 연결)
  C++: cpp-httplib 추가, /api/current 엔드포인트 구현
  React: Vite로 프로젝트 생성, /api/current 폴링해서 숫자 표시
  완료 기준: 브라우저에서 CPU/메모리 숫자가 1초마다 갱신됨
  주의: CORS 설정 필요 (Vite는 다른 포트에서 실행)

### 2단계 — 실시간 그래프
  C++: RingBuffer flush 방식 수정 (비우지 않는 슬라이딩 윈도우)
       /api/system/history, /api/process/top, /api/targets 구현
  React: Recharts LineChart로 CPU/메모리/네트워크 실시간 그래프
         프로세스 테이블, 타겟 카드
  완료 기준: 120초 실시간 그래프가 흐름

### 3단계 — 과거 데이터 조회
  C++: /api/history/* 엔드포인트 구현
       서버 전용 DuckDB Connection 추가
  React: 날짜 선택 UI, 시간 범위 선택 UI
         선택한 날짜/범위에 따라 그래프 전환
  완료 기준: 날짜 선택 → 해당 날짜 요약/상세 그래프 표시

### 4단계 — 마무리
  React 빌드 → dist/ → C++ 서버가 서빙 (CORS 불필요)
  graceful shutdown (svr.stop() + serverThread.join())
  Postman 컬렉션 작성 (API 시연용)
  완료 기준: 단일 exe 실행 → localhost:8080으로 대시보드 접근

## React 화면 구성
```
App.tsx
  ├── [탭 1] 실시간 대시보드
  │     ├── SystemPanel    ← UI_API.md 1번, 2번
  │     │     ├── 실시간 그래프 (120초, CPU/메모리/네트워크/디스크)
  │     │     └── 현재값 텍스트/표
  │     ├── ProcessPanel   ← UI_API.md 2번
  │     │     └── 현재 Top N 프로세스 테이블
  │     └── TargetPanel    ← UI_API.md 2번
  │           └── 타겟별 카드 (실행 중/중지, 실시간 수치)
  │
  └── [탭 2] 과거 기록                ← UI_API.md 3번, 4번
        ├── 날짜 선택기
        ├── 선택 날짜 요약 (SummaryStore)  ← UI_API.md 3번
        └── 상세 조회 (항목별 탭)          ← UI_API.md 4번
              ├── 시스템 상세 그래프
              ├── 프로세스 상세 그래프
              └── 타겟 상세 그래프
```
