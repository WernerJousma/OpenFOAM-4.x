/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2016 OpenFOAM Foundation
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

#include "volRegion.H"
#include "volFields.H"

// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

template<class Type>
bool Foam::functionObjects::fieldValues::volRegion::validField
(
    const word& fieldName
) const
{
    typedef GeometricField<Type, fvPatchField, volMesh> vf;

    if (obr_.foundObject<vf>(fieldName))
    {
        return true;
    }

    return false;
}


template<class Type>
Foam::tmp<Foam::Field<Type>>
Foam::functionObjects::fieldValues::volRegion::setFieldValues
(
    const word& fieldName,
    const bool mustGet
) const
{
    typedef GeometricField<Type, fvPatchField, volMesh> vf;

    if (obr_.foundObject<vf>(fieldName))
    {
        return filterField(obr_.lookupObject<vf>(fieldName));
    }

    if (mustGet)
    {
        FatalErrorInFunction
            << "Field " << fieldName << " not found in database"
            << abort(FatalError);
    }

    return tmp<Field<Type>>(new Field<Type>(0.0));
}


template<class Type>
Type Foam::functionObjects::fieldValues::volRegion::processValues
(
    const Field<Type>& values,
    const scalarField& V,
    const scalarField& weightField
) const
{
    Type result = Zero;
    switch (operation_)
    {
        case opSum:
        {
            result = gSum(values);
            break;
        }
        case opSumMag:
        {
            result = gSum(cmptMag(values));
            break;
        }
        case opAverage:
        {
            result = gSum(values)/nCells_;
            break;
        }
        case opWeightedAverage:
        {
            result = gSum(weightField*values)/gSum(weightField);
            break;
        }
        case opVolAverage:
        {
            result = gSum(V*values)/volume_;
            break;
        }
        case opWeightedVolAverage:
        {
            result = gSum(weightField*V*values)/gSum(weightField*V);
            break;
        }
        case opVolIntegrate:
        {
            result = gSum(V*values);
            break;
        }
        case opMin:
        {
            result = gMin(values);
            break;
        }
        case opMax:
        {
            result = gMax(values);
            break;
        }
        case opCoV:
        {
            Type meanValue = gSum(values*V)/volume_;

            const label nComp = pTraits<Type>::nComponents;

            for (direction d=0; d<nComp; ++d)
            {
                scalarField vals(values.component(d));
                scalar mean = component(meanValue, d);
                scalar& res = setComponent(result, d);

                res = sqrt(gSum(V*sqr(vals - mean))/volume_)/mean;
            }

            break;
        }
        case opNone:
        {}
    }

    return result;
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class Type>
bool Foam::functionObjects::fieldValues::volRegion::writeValues
(
    const word& fieldName
)
{
    const bool ok = validField<Type>(fieldName);

    if (ok)
    {
        Field<Type> values(setFieldValues<Type>(fieldName));
        scalarField V(filterField(mesh().V()));
        scalarField weightField(values.size(), 1.0);

        if (weightFieldName_ != "none")
        {
            weightField = setFieldValues<scalar>(weightFieldName_, true);
        }

        Type result = processValues(values, V, weightField);

        if (Pstream::master())
        {

            // Add to result dictionary, over-writing any previous entry
            resultDict_.add(fieldName, result, true);

            if (writeFields_)
            {
                IOField<Type>
                (
                    IOobject
                    (
                        fieldName + "_" + regionTypeNames_[regionType_] + "-"
                            + regionName_,
                        obr_.time().timeName(),
                        obr_,
                        IOobject::NO_READ,
                        IOobject::NO_WRITE
                    ),
                    weightField*values
                ).write();
            }


            file()<< tab << result;

            Log << "    " << operationTypeNames_[operation_]
                << "(" << regionName_ << ") of " << fieldName
                <<  " = " << result << endl;
        }
    }

    return ok;
}


template<class Type>
Foam::tmp<Foam::Field<Type>>
Foam::functionObjects::fieldValues::volRegion::filterField
(
    const Field<Type>& field
) const
{
    return tmp<Field<Type>>(new Field<Type>(field, cellId_));
}


// ************************************************************************* //
