#pragma once

enum class WizardPage
{
	Start = 0,
	DownloadAndInstall,
	End
};

inline WizardPage& operator++(WizardPage& c)
{
	using IntType = std::underlying_type_t<WizardPage>;
	c = static_cast<WizardPage>(static_cast<IntType>(c) + 1);
	if (c == WizardPage::End)
		c = static_cast<WizardPage>(0);
	return c;
}

inline WizardPage operator++(WizardPage& c, int)
{
	const WizardPage result = c;
	++c;
	return result;
}

inline WizardPage& operator--(WizardPage& c)
{
	using IntType = std::underlying_type_t<WizardPage>;
	c = static_cast<WizardPage>(static_cast<IntType>(c) - 1);
	if (c == WizardPage::Start)
		c = static_cast<WizardPage>(0);
	return c;
}

inline WizardPage operator--(WizardPage& c, int)
{
	const WizardPage result = c;
	--c;
	return result;
}
