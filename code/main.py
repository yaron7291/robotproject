import hashlib
import random
import socket
import threading
import time
from Crypto.Cipher import AES
import cv2

import numpy as np
from picamera2 import Picamera2, Preview

robot_status = ""
msg = ""


def encrypt_aes_ecb(message, key):
    padded_message = message.ljust(16)
    cipher = AES.new(key, AES.MODE_ECB)
    return cipher.encrypt(padded_message.encode())


def decrypt_aes_ecb(encrypted_data, key):
    cipher = AES.new(key, AES.MODE_ECB)
    decrypted_bytes = cipher.decrypt(encrypted_data)
    return decrypted_bytes.decode().strip()


def send_mgs(command, server_socket, robot_ip, port, aes_key):
    encrypted_packet = encrypt_aes_ecb(command, aes_key)
    print(f"🔒 Sending encrypted command: {command}")
    server_socket.sendto(encrypted_packet, (robot_ip, port))
    print("Sent!")


def receive_msg(server_socket, aes_key):
    data, addr = server_socket.recvfrom(16)
    decrypted_msg = decrypt_aes_ecb(data, aes_key)
    print(f"🔓 Decrypted message received from {addr[0]}: [{decrypted_msg}]")
    return decrypted_msg


def background_listener(server_socket, aes_key):
    global robot_status, msg
    while True:
        try:
            response = receive_msg(server_socket, aes_key)
            if response:
                robot_status = response
        except Exception as e:
            print(f"Exception occured in listener: {e}")
            break


def get_shared_key_dh(port, p, g):
    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.bind(("", port))

    print("Waiting for Robot broadcast...")
    while True:
        data, addr = server.recvfrom(1024)
        message = data.decode().strip()

        if message.startswith("ROBOT_START:"):
            esp32_ip = addr[0]
            try:
                public_A = int(message.split(":")[1])
                print(
                    f"Detected Robot at {esp32_ip}. Received Public A:"
                    f" {public_A}"
                )
                break
            except (ValueError, IndexError):
                continue
    my_secret_b = random.randint(1000, 5000)
    my_public_B = pow(g, my_secret_b, p)
    server.sendto(str(my_public_B).encode(), (esp32_ip, port))
    print(f"Sent Public B ({my_public_B}) back to Robot.")
    shared_key_K = pow(public_A, my_secret_b, p)
    return shared_key_K, esp32_ip, server


def Findsame_2DSearch(image1: np.ndarray, image2: np.ndarray):
    """חיפוש 2D המתגבר על הבדלי גובה (Y) שנגרמים מחוסר יישור פיזי או כיול לא מודייק"""
    gray1 = cv2.cvtColor(image1, cv2.COLOR_BGR2GRAY)
    gray2 = cv2.cvtColor(image2, cv2.COLOR_BGR2GRAY)

    h, w = gray1.shape

    # 1. מציאת הנקודה/הפינה הכי בולטת בתמונה שמאל
    corners = cv2.goodFeaturesToTrack(
        gray1, maxCorners=1, qualityLevel=0.01, minDistance=30
    )
    if corners is None:
        return -1, -1, -1

    x_c, y_c = int(corners[0][0][0]), int(corners[0][0][1])

    tw, th = 50, 50
    x_s = max(0, min(w - tw, x_c - tw // 2))
    y_s = max(0, min(h - th, y_c - th // 2))

    template = gray1[y_s : y_s + th, x_s : x_s + tw]

    # 2. חיפוש בכל התמונה הימנית (מאפשר גמישות ב-X וב-Y)
    result = cv2.matchTemplate(gray2, template, cv2.TM_CCOEFF_NORMED)
    _, max_val, _, max_loc = cv2.minMaxLoc(result)

    if max_val < 0.45:
        return -1, -1, -1

    bestx_right = max_loc[0]
    besty_right = max_loc[1]

    # הפרש ה-X משמש לחישוב הדיספריטי והמרחק
    disparity = abs(x_s - bestx_right)

    return x_s, y_s, bestx_right, besty_right, disparity


def main():
    global robot_status, msg
    smoothed_distance = 1000
    p = 104729
    g = 5
    port = 4210

    shared_key, robot_ip, server_socket = get_shared_key_dh(port, p, g)
    aes_key = hashlib.sha256(str(shared_key).encode()).digest()[:16]

    print("\n" + "=" * 30)
    print("🔑 Successfully Connected!")
    print(f"Robot IP: {robot_ip}")
    print("=" * 30)

    slow_down_msg = "slow down"
    stop_msg = "stop"
    rotate_msg = "rotate"
    good_msg = "good"

    base = 65
    f = 870
    picam1 = Picamera2(1)
    picam0 = Picamera2(0)

    picam0.start()
    picam1.start()
    time.sleep(3)

    listen_thread = threading.Thread(
        target=background_listener, args=(server_socket, aes_key), daemon=True
    )
    listen_thread.start()

    distances_history = []
    i = 0

    while robot_status != "closed":
        img0 = picam0.capture_array()
        img1 = picam1.capture_array()
        if robot_status == "rotating":
            time.sleep(0.05)
            distances_history.clear()
            continue


        if img0.shape[2] == 4:
            img0 = cv2.cvtColor(img0, cv2.COLOR_RGBA2BGR)
            img1 = cv2.cvtColor(img1, cv2.COLOR_RGBA2BGR)
        else:
            img0 = cv2.cvtColor(img0, cv2.COLOR_RGB2BGR)
            img1 = cv2.cvtColor(img1, cv2.COLOR_RGB2BGR)

        res = Findsame_2DSearch(img0, img1)

        if res[0] != -1:
            x_s, y_s, bestx_r, besty_r, disparity = res

            # ציור ריבוע ירוק בשמאל וריבוע אדום בימין
            tw, th = 50, 50
            cv2.rectangle(
                img0, (x_s, y_s), (x_s + tw, y_s + th), (0, 255, 0), 2
            )
            cv2.rectangle(
                img1, (bestx_r, besty_r), (bestx_r + tw, besty_r + th), (0, 0, 255), 2
            )

            if disparity >= 3:
                distance = (f * base) / disparity

                if 50 < distance < 3000:
                    distances_history.append(distance)
                    if len(distances_history) > 5:
                        distances_history.pop(0)
                    smoothed_distance = np.median(distances_history)

                    cv2.putText(
                        img0,
                        f"Dist: {smoothed_distance:.1f}mm",
                        (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        1,
                        (0, 255, 0),
                        2,
                    )
                    print(f"Smoothed Distance: {smoothed_distance:.2f} mm")

                    ## טיפול במקרים
                    if smoothed_distance < 350:
                        if robot_status != "rotating":
                            print(f"rotate: {i}")
                            msg = rotate_msg
                            send_mgs(msg, server_socket, robot_ip, port, aes_key)
                            robot_status = "rotating"
                        else:
                            print(f"the robot still roating {i}")
                    elif smoothed_distance < 450:
                        if robot_status != "stopped":
                            print(f"stop: {i}")
                            msg = stop_msg
                            send_mgs(msg, server_socket, robot_ip, port, aes_key)
                        else:
                            print(f"the robot still stopped {i}")
                    elif smoothed_distance < 550:
                        if robot_status != "slowed":
                            print(f"slow down {i}")
                            msg = slow_down_msg
                            send_mgs(msg, server_socket, robot_ip, port, aes_key)
                        else:
                            print(f"the robot still slow down {i}")
                    elif smoothed_distance > 750:
                        if robot_status != "staygood":
                            print(f"good: {i}")
                            msg = good_msg
                            send_mgs(msg, server_socket, robot_ip, port, aes_key)
                        else:
                            print(f"the robot still good {i}")
        # ציור קווי עזר אופקיים צהובים כדי לעקוב ויזואלית אחרי היישור ב-Y
        for y in range(0, img0.shape[0], 50):
            cv2.line(img0, (0, y), (img0.shape[1], y), (0, 255, 255), 1)
            cv2.line(img1, (0, y), (img1.shape[1], y), (0, 255, 255), 1)

        cv2.imshow("Left Camera (Template)", img0)
        cv2.imshow("Right Camera (Match)", img1)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

        i += 1

    picam0.stop()
    picam1.stop()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()