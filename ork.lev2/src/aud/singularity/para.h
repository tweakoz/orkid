class Fil4Paramsect
{
	public:

	enum { SECT, FREQ, BAND, GAIN };

	inline void init (void)
	{
		_f = 0.25f;
		_b = _g = 1.0f;
		_a = _s1 = _s2 = _z1 = _z2 = 0.0f;
	}

	inline bool proc (int k, float *sig, float f, float b, float g)
	{
		float s1, s2, d1, d2, a, da, x, y;
		bool  u2 = false;

		s1 = _s1;
		s2 = _s2;
		a = _a;
		d1 = 0;
		d2 = 0;
		da = 0;

		if (f != _f)
		{
			if      (f < 0.5f * _f) f = 0.5f * _f;
			else if (f > 2.0f * _f) f = 2.0f * _f;
			_f = f;
			_s1 = -cosf (6.283185f * f);
			d1 = (_s1 - s1) / k;
			u2 = true;
		}

		if (g != _g)
		{
			if      (g < 0.5f * _g) g = 0.5f * _g;
			else if (g > 2.0f * _g) g = 2.0f * _g;
			_g = g;
			_a = 0.5f * (g - 1.0f);
			da = (_a - a) / k;
			u2 = true;
		}

		if (b != _b)
		{
			if      (b < 0.5f * _b) b = 0.5f * _b;
			else if (b > 2.0f * _b) b = 2.0f * _b;
			_b = b;
			u2 = true;
		}

		if (u2)
		{
			b *= 7 * f / sqrtf (g);
			_s2 = (1 - b) / (1 + b);
			d2 = (_s2 - s2) / k;
		}

		while (k--)
		{
			s1 += d1;
			s2 += d2;
			a += da;
			x = *sig;
			y = x - s2 * _z2;
			*sig++ -= a * (_z2 + s2 * y - x);
			y -= s1 * _z1;
			_z2 = _z1 + s1 * y;
			_z1 = y + 1e-10f;
		}
//#ifndef NO_NAN_PROTECTION
		if (isnan(_z1)) _z1 = 0;
		if (isnan(_z2)) _z2 = 0;
//#endif
		return u2;
	}

	inline float s1 () const { return _s1 * (1.f + _s2); }
	inline float s2 () const { return _s2; }
	inline float g0 () const { return .5f * (_g - 1.f) * (1.f - _s2); }

	private:

	float  _f, _b, _g;
	float  _s1, _s2, _a;
	float  _z1, _z2;
};

