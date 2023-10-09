#pragma once

enum class WizardPage
{
	/**
	 * \brief The first page with basic actions the user can choose.
	 */
	Start = 0,
	/**
	 * \brief USer agreed to download and install right now.
	 */
	DownloadAndInstall,
	/**
	 * \brief Single update available, displays single summary.
	 */
	SingleVersionSummary,
	/**
	 * \brief Multiple new version available, display list and details.
	 */
	MultipleVersionsOverview,
	/**
	 * \brief The last possible wizard page.
	 */
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