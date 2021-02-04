
#pragma once
struct Image {
  const char *name; 
  const char data[134400];
  static constexpr auto NumImages = 2;
  static const Image Images[NumImages];
};
