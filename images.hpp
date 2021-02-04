
#pragma once
struct Image {
  const char *name; 
  const char data[134400];
  static constexpr auto NumImages = 4;
  static const Image Images[NumImages];
};
