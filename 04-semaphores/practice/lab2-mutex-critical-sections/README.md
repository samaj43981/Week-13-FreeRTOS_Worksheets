# แลปที่ 2: การใช้ Mutex เพื่อปกป้อง Critical Sections

**เป้าหมาย:** เรียนรู้วิธีการใช้ Mutex (Mutual Exclusion) เพื่อป้องกันปัญหา Race Condition ที่เกิดจากการเข้าถึงทรัพยากรที่ใช้ร่วมกัน (Shared Resource) จากหลาย Task พร้อมกัน

**เวลาโดยประมาณ:** 45 นาที

---

## 🛠️ อุปกรณ์ที่ต้องใช้

1.  บอร์ด ESP32
2.  สาย USB
3.  โปรแกรม ESP-IDF และ VS Code

---

## 📖 ทฤษฎีเบื้องต้น

### Critical Sections และ Race Conditions

-   **Critical Section:** คือส่วนของโค้ดที่เข้าถึง Shared Resource (เช่น ตัวแปร Global, Hardware Peripheral) ซึ่งจะต้องไม่ถูกขัดจังหวะโดย Task อื่นที่เข้าถึงทรัพยากรเดียวกัน
-   **Race Condition:** คือปัญหาที่เกิดขึ้นเมื่อมีตั้งแต่ 2 Tasks ขึ้นไปพยายามเข้าถึงและแก้ไข Shared Resource ในเวลาเดียวกัน ทำให้ผลลัพธ์ที่ได้ผิดเพี้ยนไปจากที่คาดหวัง

### Mutex คืออะไร?

-   **Mutex (Mutual Exclusion):** คือเครื่องมือใน FreeRTOS ที่ทำงานคล้ายกับ Binary Semaphore แต่ถูกออกแบบมาเพื่อ "ปกป้อง" Shared Resource โดยเฉพาะ
-   **หลักการทำงาน:** Task ที่ต้องการเข้าถึง Critical Section จะต้อง "ครอบครอง" (Take) Mutex ให้ได้ก่อน เมื่อใช้งานเสร็จแล้วจะต้อง "ปล่อย" (Give) Mutex คืน เพื่อให้ Task อื่นสามารถเข้ามาใช้งานได้
-   **คุณสมบัติสำคัญ:** Mutex ใน FreeRTOS มีกลไก **Priority Inheritance** เพื่อช่วยแก้ปัญหา **Priority Inversion**

### Priority Inversion คืออะไร?

-   เป็นปัญหาที่ Task ที่มีความสำคัญสูง (High Priority) ต้องรอ Task ที่มีความสำคัญต่ำกว่า (Low Priority) ทำงานให้เสร็จก่อน
-   **สถานการณ์:**
    1.  Task L (Low) ครอบครอง Mutex อยู่
    2.  Task H (High) ต้องการใช้ Mutex เดียวกัน แต่ถูก Block เพราะ Task L ยังไม่ปล่อย
    3.  ในขณะนั้น Task M (Medium) ที่พร้อมทำงานและมีความสำคัญสูงกว่า Task L เข้ามาทำงาน (Preempt)
    4.  ผลคือ Task H ต้องรอทั้ง Task L และ Task M ซึ่งทำให้ระบบไม่ตอบสนองตาม Priority ที่ควรจะเป็น

### Priority Inheritance แก้ปัญหานี้ได้อย่างไร?

-   เมื่อ Task H พยายามจะ Take Mutex ที่ Task L ถืออยู่, FreeRTOS จะ "เพิ่ม" Priority ของ Task L ให้สูงเท่ากับ Task H ชั่วคราว
-   ทำให้ Task M ไม่สามารถเข้ามา Preempt Task L ได้
-   Task L จะทำงานของมันใน Critical Section จนเสร็จและปล่อย Mutex
-   เมื่อปล่อย Mutex แล้ว Priority ของ Task L จะกลับมาเป็นปกติ และ Task H จะได้ทำงานต่อไป

---

## 🧪 ขั้นตอนการทดลอง

### ส่วนที่ 1: จำลองปัญหา Race Condition

ในส่วนนี้ เราจะสร้าง 2 Tasks ที่พยายามจะแก้ไขตัวแปร Global เดียวกันโดยไม่มีการป้องกันใดๆ เพื่อให้เห็นปัญหา Race Condition

1.  **สร้างโปรเจกต์:**
    -   สร้างโฟลเดอร์ `lab2-mutex-critical-sections` ภายใน `04-semaphores/practice/`
    -   สร้างไฟล์ `mutex_demo.c` และ `CMakeLists.txt`

2.  **เขียนโค้ด (mutex_demo.c):**
    -   สร้างตัวแปร Global ชื่อ `shared_resource`
    -   สร้าง 2 Tasks ที่มี Priority เท่ากัน
    -   ในแต่ละ Task ให้วน Loop อ่านค่า `shared_resource`, เพิ่มค่าขึ้น 1, และเขียนค่ากลับ (จำลองการทำงานที่ซับซ้อน)
    -   ใส่ `vTaskDelay` เล็กน้อยใน Loop เพื่อให้เกิดการสลับการทำงาน (Context Switching) ได้ง่ายขึ้น
    -   พิมพ์ค่าของ `shared_resource` ออกมาดู

3.  **สังเกตผลลัพธ์:**
    -   คุณจะเห็นว่าค่าของ `shared_resource` ที่พิมพ์ออกมานั้นไม่ถูกต้อง และมีการเพิ่มค่าที่ทับซ้อนกัน (เช่น Task 1 อ่านค่าได้ 5, Task 2 ก็อ่านได้ 5, ทั้งคู่เพิ่มค่าเป็น 6 แล้วเขียนกลับ ทำให้ค่าสุดท้ายเป็น 6 แทนที่จะเป็น 7)

### ส่วนที่ 2: แก้ปัญหาด้วย Mutex

เราจะใช้ Mutex เพื่อปกป้อง `shared_resource`

1.  **แก้ไขโค้ด (mutex_demo.c):**
    -   สร้าง Mutex โดยใช้ `xSemaphoreCreateMutex()`
    -   ก่อนจะเข้าถึง `shared_resource` ในแต่ละ Task ให้เรียก `xSemaphoreTake(mutex, portMAX_DELAY)`
    -   หลังจากใช้งาน `shared_resource` เสร็จแล้ว ให้เรียก `xSemaphoreGive(mutex)`
    -   **สำคัญ:** ต้องแน่ใจว่า `xSemaphoreGive()` ถูกเรียกเสมอ แม้ว่าจะมี Error เกิดขึ้นใน Critical Section ก็ตาม

2.  **คอมไพล์และทดลอง:**
    -   Flash โค้ดลง ESP32
    -   สังเกตผลลัพธ์อีกครั้ง

3.  **วิเคราะห์ผลลัพธ์:**
    -   ค่าของ `shared_resource` จะเพิ่มขึ้นทีละ 1 อย่างถูกต้องเสมอ เพราะ Mutex ทำให้มีเพียง Task เดียวเท่านั้นที่สามารถเข้าถึง Critical Section ได้ ณ เวลาใดเวลาหนึ่ง

---

## 📝 แบบฝึกหัดและคำถาม

1.  **ทดลอง Priority Inversion:**
    -   สร้าง Task ที่ 3 ที่มี Priority สูงกว่า 2 Tasks แรก แต่ไม่ยุ่งเกี่ยวกับ Mutex
    -   ให้ Task ที่ Priority ต่ำสุดครอบครอง Mutex และ Delay เป็นเวลานาน
    -   ให้ Task ที่ Priority สูงสุดพยายามจะ Take Mutex
    -   สังเกตว่า Task ที่ Priority ปานกลางได้ทำงานหรือไม่ และ Task Priority สูงสุดต้องรอนานแค่ไหน
    -   *(คำแนะนำ: FreeRTOS Mutex มี Priority Inheritance ในตัว การจะเห็นปัญหานี้อาจจะต้องใช้ Semaphore ธรรมดาแทน Mutex ในการทดลอง)*

2.  **Deadlock คืออะไร?**
    -   ลองจินตนาการสถานการณ์ที่ Task 1 ต้องการ Resource A และ B โดยถือ A อยู่และรอ B ในขณะที่ Task 2 ถือ B อยู่และรอ A สถานการณ์นี้เรียกว่า Deadlock ลองอธิบายว่าจะเกิดขึ้นได้อย่างไรในการเขียนโค้ด

3.  **Mutex กับ Binary Semaphore ต่างกันอย่างไร?**
    -   นอกเหนือจาก Priority Inheritance แล้ว มีความแตกต่างในเชิงการใช้งานอื่นๆ อีกหรือไม่? (คำใบ้: ใครสามารถ "Give" เซมาฟอร์ได้บ้าง?)

---

## ✅ Checklist

-   [ ] สร้างโปรเจกต์และไฟล์ที่จำเป็น
-   [ ] เขียนโค้ดจำลอง Race Condition และเห็นปัญหา
-   [ ] สร้าง Mutex และนำไปใช้ในโค้ด
-   [ ] แก้ปัญหา Race Condition ได้สำเร็จ
-   [ ] ตอบคำถามและทำแบบฝึกหัด

