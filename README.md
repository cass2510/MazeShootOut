# 🔫 FPS Maze Escape

**C++ & raylib** 기반 3D FPS 스타일 미로 탈출 게임

절차적 맵 생성, 총격전, 보스 AI, 시각 이펙트까지 직접 설계한 프로젝트입니다.

---

## 프로젝트 소개

**Maze Escape**는 C++과 raylib를 활용하여 구현한 3D FPS 게임으로, 절차적으로 생성되는 미로에서 적과 보스를 물리치고 탈출구를 찾아 클리어하는 것을 목표로 합니다.

### 주요 특징

* **절차적 맵 생성 → 경로 유효성 검사**

  * 20×20 격자 미로를 랜덤으로 생성 후 BFS 기반 경로 검사(`HasPath`)로 출발\~출구 연결 보장
  * 유효 경로가 없으면 자동 재생성
* **2종의 AI 적 (일반 & 보스) + 전투 시스템**

  * 일반: 플레이어에게 직진, 접촉 시 데미지
  * 보스: `HasLOS()`로 시야 판정, 일정 쿨다운마다 탄환 발사
* **실시간 총알 충돌 판정 & 궤적 이펙트**

  * `Bullet` 구조체에 위치·방향·trail 저장 → `DrawLine3D`로 연속 궤적 렌더링
  * 벽·적 충돌 시 임팩트(`Impact`) 생성 및 파티클 전개
* **피격 연출 & 혈흔 파티클**

  * `damageEffect`로 화면 붉은 오버레이
  * `BloodParticle`로 y축 분산 혈흔 파티클 12개 생성 → 속도·감쇠 처리
* **미니맵 HUD & 상태 표시**

  * `DrawMiniMap`으로 우측 상단 실시간 미니맵 렌더링
  * 체력, 점수, 남은 적 수, 클리어 타이머 등 HUD 출력
* **게임 클리어 / 오버 조건**

  * 체력 ≤ 0 → GAME OVER
  * 출구 도달 → STAGE CLEAR & 클리어 시간 표시

---

## 주요 구조체 및 코드 예시

### 1. `GenerateMap()` + `HasPath()`

```cpp
// 절차적 맵 생성 & 경로 보장
do { GenerateMap(); }
while (!HasPath(1,1,MAP_SIZE-2,MAP_SIZE-2));
```

* `GenerateMap()`: 가장자리 벽 생성 + 내부 30% 확률 벽 배치
* `HasPath()`: BFS로 (1,1) → (MAP\_SIZE-2,MAP\_SIZE-2) 경로 확인

### 2. `Bullet` & 궤적 이펙트

```cpp
struct Bullet {
    Vector3 pos, dir;      // 위치·방향
    bool active;           // 활성화 여부
    std::vector<Vector3> trail; // 궤적 좌표 저장
};

// 매 프레임 이동 → trail.push_back → 궤적 렌더링
b.trail.push_back(b.pos);
b.pos = Vector3Add(b.pos, Vector3Scale(b.dir, BULLET_SPEED));
```

### 3. 보스 AI

```cpp
if (HasLOS(e.pos, player.pos)) {
    if (e.fireCooldown <= 0.0f) {
        enemyBullets.push_back({ e.pos, dir, true });
        e.fireCooldown = 1.0f + (rand()%100)/100.0f * 2.0f;
    }
}
```

* `HasLOS()`: 광선 추적으로 벽 방해 여부 확인
* 쿨다운 범위: 1\~3초 랜덤 재설정

### 4. 파티클 & 혈흔

```cpp
for (auto& b : bloods) {
    b.pos = Vector3Add(b.pos, b.vel);
    b.vel = Vector3Scale(b.vel, 0.92f);
    b.life -= 0.03f;
}
```

* `BloodParticle`: 위치·속도·수명 관리
* 수명 0 되면 제거 (`std::remove_if`)

---

## 게임 실행 방법

1. **MazeShootOut.zip** 압축 해제
2. 실행 파일(`MazeShootOut.exe`)과 함께 `raylib.dll`, 텍스처 파일(`brick.png`, `boss.png`, `enemy.png`, `aid.png`, `exit.png`)을 동일 폴더에 위치
3. **Windows**: 실행 파일 더블 클릭
4. **Linux/Mac**: 터미널에서 `./MazeShootOut`

### 조작 키

* 이동: `W/A/S/D`
* 시점 회전: 마우스 이동
* 발사: 마우스 왼쪽 버튼
* 종료: `ESC`
