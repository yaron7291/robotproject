void CheckSpeed()
{
  // הגדרת ערך PWM מינימלי כדי למנוע מהמנוע להיעצר ולהתקע (Dead Zone)
  const int minPWM = 75; 

  // === מנוע שמאלי ===
  if(targetSpeedleft > 0)
  {
    leftdownvalue = 0;
    if (targetSpeedleft > currentSpeedleft){
      // צעדי תיקון מתונים יותר (5 במקום 10) כדי למנוע רעידות
      if (targetSpeedleft - currentSpeedleft > 2) leftupvalue += 5; 
      else leftupvalue += 1;
    }
    else{
      if (currentSpeedleft - targetSpeedleft > 2) leftupvalue -= 5;
      else leftupvalue -= 1;
    }
    
    // אם המנוע אמור לנסוע, נשמור עליו מעל ה-PWM המינימלי כדי שלא ייעצר באמצע
    if (leftupvalue < minPWM && leftupvalue > 0) {
        leftupvalue = minPWM;
    }
  }
  else
  {
    leftupvalue = 0;
    if (targetSpeedleft < currentSpeedleft){
      if (currentSpeedleft - targetSpeedleft > 2) leftdownvalue -= 5;
      else leftdownvalue -= 1;
    }
    else{
      if (targetSpeedleft - currentSpeedleft > 2) leftdownvalue += 5;
      else leftdownvalue += 1;
    }
    
    if (leftdownvalue < minPWM && leftdownvalue > 0) {
        leftdownvalue = minPWM;
    }
  }

  // === מנוע ימני ===
  if(targetSpeedright > 0)
  {
    rightdownvalue = 0;
    if (targetSpeedright > currentSpeedright){
      if (targetSpeedright - currentSpeedright > 2) rightupvalue += 5;
      else rightupvalue += 1;
    }
    else{
      if (currentSpeedright - targetSpeedright > 2) rightupvalue -= 5;
      else rightupvalue -= 1;
    }
    
    if (rightupvalue < minPWM && rightupvalue > 0) {
        rightupvalue = minPWM;
    }
  }
  else
  {
    rightupvalue = 0;
    if (targetSpeedright < currentSpeedright){
      if (currentSpeedright - targetSpeedright > 2) rightdownvalue -= 5;
      else rightdownvalue -= 1;
    }
    else{
      if (targetSpeedright - currentSpeedright > 2) rightdownvalue += 5;
      else rightdownvalue += 1;
    }
    
    if (rightdownvalue < minPWM && rightdownvalue > 0) {
        rightdownvalue = minPWM;
    }
  }

  // הגנה על גבולות ה-PWM (0-255)
  leftupvalue    = constrain(leftupvalue, 0, 255);
  leftdownvalue  = constrain(leftdownvalue, 0, 255);
  rightupvalue   = constrain(rightupvalue, 0, 255);
  rightdownvalue = constrain(rightdownvalue, 0, 255);

  // כתיבת הערכים למנועים
  analogWrite(leftup, leftupvalue);
  analogWrite(leftdown, leftdownvalue);
  analogWrite(rightup, rightupvalue);
  analogWrite(rightdown, rightdownvalue);
}