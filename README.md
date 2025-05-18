🔫 FPS Maze Escape - raylib + C++
C++과 raylib으로 만든 3D FPS 스타일 미로 탈출 게임
절차적 맵 생성, 총격전, 보스 AI, 시각 이펙트까지 직접 설계한 프로젝트

프로젝트 소개
Maze Escape는 C++과 raylib을 활용한 3D FPS 게임으로,
절차적으로 생성되는 미로에서 적과 보스를 물리치고 탈출구를 찾아 클리어하는 것을 목표로 합니다.

본 게임은 단순한 시각화 예제를 넘어, 다음과 같은 구조적 요소를 포함합니다:

절차적 맵 생성 + 경로 유효성 검사

2종의 AI 적 (일반, 보스)과 전투 시스템

실시간 총알 충돌 판정과 궤적 이펙트

피격 연출, 임팩트, 혈흔 등 다양한 시각 효과

미니맵 HUD, 체력·점수 시스템, 클리어 타이머

주요 개발 요소
🧱 절차적 미로 생성 + 경로 보장
20×20 맵을 랜덤으로 생성 후, BFS 기반으로 시작~출구 간 경로 유효성 검사

경로가 없는 맵은 자동으로 재생성

벽 비율, 가장자리 감싸기 등 게임성 고려

cpp
복사
편집
do {
    GenerateMap();
} while (!HasPath(1, 1, MAP_SIZE - 2, MAP_SIZE - 2));
🔫 총격 시스템: 탄환, 충돌, 궤적
탄환은 Bullet 구조체로 관리, 이동마다 trail 저장 → 선형 궤적 시각화

벽이나 적과 충돌 시 impact 이펙트 생성

피격 시 적에게 혈흔 파티클 12개 분산 발생 (방향·속도 랜덤)

cpp
복사
편집
b.trail.push_back(b.pos);
b.pos = Vector3Add(b.pos, Vector3Scale(b.dir, BULLET_SPEED));
🧠 AI 적 & 보스
일반 적은 플레이어에게 직진하며 접촉시 데미지

보스는 HasLOS()로 직선 시야 판정 후, 일정 시간마다 탄환 발사

보스는 체력이 높고 속도는 느리며, 별도 쿨다운 적용

cpp
복사
편집
if (HasLOS(e.pos, player.pos)) {
    if (e.fireCooldown <= 0.0f) {
        enemyBullets.push_back({ e.pos, dir, true });
        e.fireCooldown = 1.0f + (rand() % 100) / 100.0f * 2.0f;
    }
}
💥 시각 효과: 파티클, 혈흔, 데미지 오버레이
총알 피격 시 붉은 화면 오버레이 (damageEffect)

파티클은 BloodParticle 구조체로 이동·감쇠

적 사망 시 혈흔 파편이 원형으로 분산되어 시각적 타격감 강화

cpp
복사
편집
for (auto& b : bloods) {
    b.pos = Vector3Add(b.pos, b.vel);
    b.vel = Vector3Scale(b.vel, 0.92f);
    b.life -= 0.03f;
}
🗺 미니맵 + HUD
우측 상단에 실시간 미니맵 표시 (DrawMiniMap)

플레이어, 적(보스/일반), 아이템, 출구까지 색상별 표시

체력, 점수, 남은 적 수 등 게임 진행 정보를 직관적으로 HUD에 출력

🏁 게임 클리어 / 종료 조건
플레이어 체력 ≤ 0 → GAME OVER

출구 근처 도달 시 → STAGE CLEAR와 클리어 시간 출력

탈출 조건: 보스와 적을 무시하고 빠르게 클리어할 수도 있음

cpp
복사
편집
if (!gameOver && !gameClear && Vector3Distance(exitPos, player.pos) < 0.8f) {
    gameClear = true;
    clearTime = GetTime() - startTime;
}
설계 포인트 요약
요소	설명
그래픽 표현	DrawModelEx, DrawBillboard, DrawSphere 등 raylib의 3D 렌더링 API를 적극 활용
구조화	Player, Enemy, Bullet, Item, Impact, BloodParticle 등 명확한 구조체 기반 설계
이벤트 처리	탄환 이동/충돌, 보스 AI 동작, 파티클 수명 관리 등은 Update 루프 내에서 프레임 단위로 처리
최적화	trail 및 파티클 등 불필요한 오브젝트는 std::remove_if로 정리하여 성능 관리

🕹 실행 방법
이 저장소를 다운로드하거나, 제공된 MazeShootOut.zip 파일의 압축을 해제합니다.

폴더 내의 MazeShootOut.exe 파일을 더블 클릭하면 게임이 실행됩니다.

WASD 키로 이동하고, 마우스로 조준하며, 왼쪽 클릭으로 공격하세요.

게임은 ESC 키로 종료할 수 있습니다.

✅ raylib.dll 및 필요한 이미지 리소스들이 모두 포함되어 있어, 별도의 설치나 설정 없이 실행됩니다.
