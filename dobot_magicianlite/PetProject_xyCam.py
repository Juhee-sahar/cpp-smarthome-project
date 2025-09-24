# yellow_three_coords.py
# ----------------------------------------------------------
# 웹캠에서 노란색 물체를 찾아, 감지된 모든 픽셀 좌표 출력
# - 출력 예: [YELLOW] N=2  #1(632,418)  #2(215,402)
# - 'q' 키로 종료
# ----------------------------------------------------------

import cv2
import numpy as np
import time

# ----------------- 기본 설정 -----------------
CAM_INDEX = 1
FRAME_W, FRAME_H = 1280, 720
FOURCC_MJPG = True

YELLOW_LOWER = (15, 100, 100)
YELLOW_UPPER = (35, 255, 255)

MIN_AREA = 800
MORPH_K = 5
PRINT_INTERVAL = 0.1  # 초당 10회 이하 출력 제한


def main():
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_DSHOW)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  FRAME_W)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_H)
    if FOURCC_MJPG:
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))

    if not cap.isOpened():
        raise RuntimeError("Failed to open camera. Try another CAM_INDEX.")

    last_print = 0.0

    while True:
        ok, frame = cap.read()
        if not ok:
            print("[WARN] Failed to read a frame.")
            break

        # --- 전처리 ---
        blur = cv2.GaussianBlur(frame, (5,5), 0)
        hsv  = cv2.cvtColor(blur, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, np.array(YELLOW_LOWER), np.array(YELLOW_UPPER))

        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (MORPH_K, MORPH_K))
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=2)

        # --- 컨투어 ---
        cnts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        objs = []
        for c in cnts:
            area = cv2.contourArea(c)
            if area < MIN_AREA:
                continue
            M = cv2.moments(c)
            if M["m00"] == 0:
                continue
            cx = int(M["m10"]/M["m00"])
            cy = int(M["m01"]/M["m00"])
            objs.append((area, cx, cy, c))

        # 면적 내림차순 정렬
        objs.sort(key=lambda t: t[0], reverse=True)

        coords = []
        for idx, (_, cx, cy, c) in enumerate(objs, start=1):
            x, y, w, h = cv2.boundingRect(c)
            cv2.rectangle(frame, (x,y), (x+w, y+h), (0,255,0), 2)
            cv2.circle(frame, (cx,cy), 6, (255,0,0), -1)
            cv2.putText(frame, f"#{idx}", (cx+8, cy-8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0,0,255), 2)
            coords.append((cx, cy))

        # 좌표 출력 (속도 제한)
        now = time.time()
        if now - last_print > PRINT_INTERVAL:
            if coords:
                parts = [f"#{i+1}({x},{y})" for i,(x,y) in enumerate(coords)]
                print(f"[YELLOW] N={len(coords)}  " + "  ".join(parts))
            last_print = now

        # 안내문 오버레이
        cv2.putText(frame, "Detecting unlimited yellow objects (q=quit)",
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (20,20,20), 3)
        cv2.putText(frame, "Detecting unlimited yellow objects (q=quit)",
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255,255,255), 1)

        # 창 표시
        cv2.imshow("yellow_multi", frame)
        cv2.imshow("mask", mask)

        # 종료
        if (cv2.waitKey(1) & 0xFF) == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()

