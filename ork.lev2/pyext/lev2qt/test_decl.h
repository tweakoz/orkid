class mytype {
public:
  mytype(const std::string& str);
  virtual ~mytype() {
  }
  std::string _val;
};

class Icecream {
public:
  Icecream(const mytype& flavor);
  virtual ~Icecream() {
  }
  const mytype& getFlavor() const;

private:
  mytype _theflavor;
};
