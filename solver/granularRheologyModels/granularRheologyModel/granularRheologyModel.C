/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 1991-2010 OpenCFD Ltd.
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "granularRheologyModel.H"
#include "FrictionModel.H"
#include "PPressureModel.H"
#include "FluidViscosityModel.H"
#include "DilatancyModel.H"
#include "surfaceInterpolate.H"
#include "mathematicalConstants.H"
#include "fvCFD.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::granularRheologyModel::granularRheologyModel
(
    const Foam::phaseModel& phasea,
    const Foam::phaseModel& phaseb,
    const Foam::volScalarField& pa,
    const Foam::dimensionedScalar& Dsmall
)
:
    alpha_(phasea.alpha()),
    phia_(phasea.phi()),
    rhoa_(phasea.rho()),
    da_(phasea.d()),
    pa_new_value(pa),
    rhob_(phaseb.rho()),
    nub_(phaseb.nu()),

    granularRheologyProperties_
    (
        IOobject
        (
            "granularRheologyProperties",
            alpha_.time().constant(),
            alpha_.mesh(),
            IOobject::MUST_READ,
            IOobject::NO_WRITE
        )
    ),
    granularRheology_(granularRheologyProperties_.lookup("granularRheology")),
    FrictionModel_
    (
        granularRheologyModels::FrictionModel::New
        (
            granularRheologyProperties_
        )
    ),
    PPressureModel_
    (
        granularRheologyModels::PPressureModel::New
        (
            granularRheologyProperties_
        )
    ),
    FluidViscosityModel_
    (
        granularRheologyModels::FluidViscosityModel::New
        (
            granularRheologyProperties_
        )
    ),
    DilatancyModel_
    (
        granularRheologyModels::DilatancyModel::New
        (
            granularRheologyProperties_
        )
    ),
    alphaMaxG_
    (
        granularRheologyProperties_.lookupOrDefault
        (
            "alphaMaxG",
            dimensionedScalar("alphaMaxG",
                          dimensionSet(0, 0, 0, 0, 0, 0, 0),
                          0.6)
        )
    ),
    mus_
    (
        granularRheologyProperties_.lookupOrDefault
        (
            "mus",
            dimensionedScalar("mus",
                          dimensionSet(0, 0, 0, 0, 0, 0, 0),
                          0.38)
        )
    ),
    mu2_
    (
        granularRheologyProperties_.lookupOrDefault
        (
            "mu2",
            dimensionedScalar("mu2",
                          dimensionSet(0, 0, 0, 0, 0, 0, 0),
                          0.64)
        )
    ),
    I0_
    (
        granularRheologyProperties_.lookupOrDefault
        (
            "I0",
            dimensionedScalar("I0",
                          dimensionSet(0, 0, 0, 0, 0, 0, 0),
                          0.3)
        )
    ),
    Bphi_
    (
        granularRheologyProperties_.lookupOrDefault
        (
            "Bphi",
            dimensionedScalar("Bphi",
                          dimensionSet(0, 0, 0, 0, 0, 0, 0),
                          0.31)
        )
    ),
    n_
    (
        granularRheologyProperties_.lookupOrDefault
        (
            "n",
            dimensionedScalar("n",
                          dimensionSet(0, 0, 0, 0, 0, 0, 0),
                          2.5)
        )
    ),
    BulkFactor_
    (
        granularRheologyProperties_.lookupOrDefault
        (
                "BulkFactor",
                dimensionedScalar("BulkFactor",
                    dimensionSet(0, 0, 0, 0, 0, 0, 0),
                    0)
        )
    ),
    alpha_c_
    (
        granularRheologyProperties_.lookupOrDefault
        (
                "alpha_c",
                dimensionedScalar("alpha_c",
                    dimensionSet(0, 0, 0, 0, 0, 0, 0),
                    0.585)
        )
    ),
    
    K_dila_
    (
        granularRheologyProperties_.lookupOrDefault
        (
                "K_dila",
                dimensionedScalar("K_dila",
                    dimensionSet(0, 0, 0, 0, 0, 0, 0),
                    0)
        )
    ),
    relaxPa_
    (
        granularRheologyProperties_.lookupOrDefault
        (
            "relaxPa",
            dimensionedScalar("relaxPa",
                          dimensionSet(0, 0, 0, 0, 0, 0, 0),
                          1)
        )
    ),
    muI_
    (
        IOobject
        (
            "muI",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", alpha_.dimensions(), 0.0)
    ),
    mua_
    (
        IOobject
        (
            "mua",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", dimensionSet(1, -1, -1, 0, 0), 0.0)
    ),
    lambda_
    (
        IOobject
        (
            "lambda",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", dimensionSet(1, -1, -1, 0, 0), 0.0)
    ),

    pa_
    (
        IOobject
        (
            "pa",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", dimensionSet(1, -1, -2, 0, 0), 0.0)
    ),
    p_p_total_
    (
        IOobject
        (
            "p_p_total",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", dimensionSet(1, -1, -2, 0, 0), 0.0)
    ),

    delta_
    (
        IOobject
        (
            "delta",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", alpha_.dimensions(), 0.0)
    ),
     nuvb_
     (
        IOobject
        (
            "nuvb",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", dimensionSet(0, 2, -1, 0, 0), 0.0)
     ),
    Iv_
    (
        IOobject
        (
            "Iv",
            alpha_.time().timeName(),
            alpha_.mesh(),
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        ),
        alpha_.mesh(),
        dimensionedScalar("zero", dimensionSet(0, 0, 0, 0, 0), 0.0)
    )
{}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::granularRheologyModel::~granularRheologyModel()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::granularRheologyModel::solve
(
    const volTensorField& gradUat,
    const volScalarField& pf,
    const dimensionedScalar& alphaSmall,
    const dimensionedScalar& Dsmall
)
{
    if (not granularRheology_)
    {
        return;
    }
    dimensionedScalar Dsmall2
    (
        "Dsmall2",
        dimensionSet(0, 0, -2, 0, 0, 0, 0),
        1e-8
    );
    Dsmall2 = sqr(Dsmall);

    // compute the particulate velocity shear rate
    //
    volSymmTensorField D = symm(gradUat);
    volScalarField magD = ::sqrt(2.0)*mag(D);
    volScalarField magD2 = pow(magD, 2);
    volScalarField patot_ = pf*scalar(0.0);
    
    //
    // Shear induced particulate pressure
    //
    pa_ = PPressureModel_->pa
    (
        pf, Bphi_, rhoa_, da_, rhob_, nub_, magD,
        alpha_, alphaMaxG_, alphaSmall
    );

    // Relaxing shear induced particulate pressure
    //  relaxPa_ controls the relaxation of pa. Low values lead to relaxed pa
    //  whereas large value are prone to numerical error
    volScalarField tau_inv_par = relaxPa_*alpha_*magD;

    fvScalarMatrix paEqn
    (
         fvm::ddt(pa_new_value)
         + fvm::div(phia_, pa_new_value, "div(phia,pa_new_value)")
         - fvm::Sp(fvc::div(phia_), pa_new_value)
        ==
        tau_inv_par*(pa_)
        -fvm::Sp(tau_inv_par, pa_new_value)
    );
    paEqn.relax();
    paEqn.solve();

    pa_new_value.max(0.0);

    pa_=pa_new_value;

    //total particle pressure(shear induced+contact contributions)
    p_p_total_ = mag(pa_new_value+pf);
    //JC why not p_p_total_ = mag(pa_+pf); ?

    //  Compute the particulate friction coefficient
    muI_ = FrictionModel_->muI(mus_, mu2_, I0_, p_p_total_, rhoa_, da_, rhob_,
                               nub_, magD, Dsmall);

// Dilatancy model
    dimensionedScalar PaMin
    (
        "PaMin",
        dimensionSet(1, -1, -2, 0, 0, 0, 0),
        5e-1
    );

    delta_ = DilatancyModel_->delta(K_dila_, alpha_c_, alpha_, magD,
 da_, rhob_, nub_, p_p_total_, PaMin);

    delta_.min( 0.5);
    delta_.max(-0.5);

    //  Compute the regularized particulate viscosity
    mua_ = muI_* p_p_total_ / pow(magD2 + Dsmall2, 0.5);

    // Compute bulk viscosity (by default BulkFactor = 0)s
    lambda_ = BulkFactor_*p_p_total_ / pow(magD2 + Dsmall2, 0.5);


    // Compute the Effective fluid viscosity
    nuvb_ = FluidViscosityModel_->nuvb(alpha_, nub_, alphaMaxG_, alphaSmall,
                                       n_);
}
// ************************************************************************* //
